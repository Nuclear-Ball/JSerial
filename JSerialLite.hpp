#pragma once
#include <iostream>
#include <utility>
#include <vector>
#include <string>
#include <cstring>
#include "JSerialUtils.h"

/* Стандарт JSerial:
 * Любая статическая память записывается просто в двоичном виде без каких-либо
 * дополнительных данных
 *
 * Динамическая память: перед блоком динамической памяти должен находится ее размер в байтах,
 * с размером ячейки равным размеру типа адреса
 *
 * Массив строк: как и у другой динамической памяти перед этим должен быть байтовый размер.
 * Но также должно быть и количество строк в массиве. После ячейки с количеством строк
 * должна распологаться таблица смещений. А после этого уже сами строки в формате блоков данных
 */

namespace JSerial{
	struct TypeStruct{
		size_t size;
		bool is_dynamic;
	};

	template<typename T> inline constexpr TypeStruct Type()
		{ return {sizeof(T), false }; }
	inline constexpr TypeStruct DynamicType()
		{ return {0, true }; }

	template <typename aT> aT CalculateTemplateSize(const std::initializer_list<TypeStruct>& types){
		static_assert(std::is_unsigned<aT>::value, "Invalid address type: must be an unsigned integer");
		size_t res = 0;
		for(const TypeStruct& i : types)
			res += i.is_dynamic ? sizeof(aT) : i.size;
		return res;
	}

	template<typename aT = size_t> class C_Write {
		static_assert(std::is_unsigned<aT>::value, "Invalid address type: must be an unsigned integer");
	private:
		aT size = 0;
		std::string data;
		aT cursor_pos = 0;

	public:
		inline C_Write() = default;
		inline explicit C_Write(const aT size)
			{ Reset(size); }

		template<typename T> inline C_Write<aT>& WriteStaticData(const T& dat){
			memcpy(&data[cursor_pos], &dat, sizeof(T));
			cursor_pos += sizeof(T);

			return *this;
		}

		template<typename T> inline C_Write<aT>& WriteDynamicData(const T* dat, aT memory_size){
			data.resize(data.size() + memory_size);
			memcpy(&data[cursor_pos], &memory_size, sizeof(aT));
			cursor_pos += sizeof(aT);
			memcpy(&data[cursor_pos], dat, memory_size * sizeof(T));
			cursor_pos += memory_size * sizeof(T);

			return *this;
		}
		inline C_Write<aT>& WriteDynamicData(const std::string& dat)
			{ return WriteDynamicData(dat.data(), dat.size()); }
		template<typename T> inline C_Write<aT>& WriteDynamicData(const std::vector<T>& dat)
			{ return WriteDynamicData(dat.data(), dat.size()); }

		C_Write<aT>& WriteStringArray(const std::string* dat, aT amount){
			aT block_size = sizeof(aT) * (amount + 1);
			for(aT i = 0; i < amount; i++)
				block_size += sizeof(aT) + dat[i].size();
			data.resize(data.size() + block_size);

			memcpy(&data[cursor_pos], &block_size, sizeof(aT));//Запись размера в байтах секции с массивом строк
			cursor_pos += sizeof(aT);
			memcpy(&data[cursor_pos], &amount, sizeof(aT)); //Количество строк
			cursor_pos += sizeof(aT);

			aT table_pos = cursor_pos;
			cursor_pos += amount * sizeof(aT);
			for(aT i = 0; i < amount; i++){
				const aT offset = cursor_pos - table_pos; //Расстояние должно быть от ячейки со смещением
				memcpy(&data[table_pos], &offset, sizeof(aT)); //Запись смещения строки из массива в таблицу смещений
				table_pos += sizeof(aT);


				const std::string& c_str = dat[i];

				const aT str_size = c_str.size();
				memcpy(&data[cursor_pos], &str_size, sizeof(aT));
				cursor_pos += sizeof(aT);

				memcpy(&data[cursor_pos], c_str.data(), c_str.size());//Запись строки
				cursor_pos += c_str.size();
			}
			return *this;
		}
		inline C_Write<aT>& WriteStringArray(const std::vector<std::string>& dat)
			{ return WriteStringArray(dat.data(), dat.size()); }

		inline void Reset(){
			data.resize(size);
			cursor_pos = 0;
		}
		inline void Reset(const aT new_size){
			size = new_size;
			Reset();
		}

		inline std::string Result()
			{ return data; }
	};
	template<typename aT = size_t> class C_Read {
		static_assert(std::is_unsigned<aT>::value, "Invalid address type: must be an unsigned integer");
	private:
		aT cursor_pos = 0;
		std::string src;
	public:
		inline C_Read() = default;
		inline explicit C_Read(const std::string& source)
			{ src = source; }

		inline void Source(const std::string& source)
			{ src = source; }

		template<typename T> inline T GetStaticData(){
			const T res = *reinterpret_cast<T*>(&src[cursor_pos]);
			cursor_pos += sizeof(T);
			return res;
		}

		template<typename T> inline aT GetDynamicDataSize()
			{ return *reinterpret_cast<aT*>(&src[cursor_pos]) / sizeof(T); }

		template<typename T> inline void GetDynamicData(T* destination, aT buffer_size){
			cursor_pos += sizeof(aT);
			memcpy(destination, &src[cursor_pos], buffer_size);
			cursor_pos += buffer_size;
		}

		template<typename T> inline std::vector<T> GetDynamicData(){
			std::vector<T> res(GetDynamicDataSize<T>());
			GetDynamicData<T>(res.data(), res.size());
			return res;
		}

		inline std::string GetDynamicData(){
			std::string res(GetDynamicDataSize<char>(), 0);
			GetDynamicData<char>(const_cast<char*>(res.data()), res.size());
			return res;
		}

		inline aT GetStringArraySize()
			{ return *reinterpret_cast<aT*>(&src[cursor_pos + sizeof(aT)]); }

		inline void GetStringArray(std::string* buffer, aT buffer_size){
			cursor_pos += sizeof(aT) * (2 + buffer_size);
			for(aT i = 0; i < buffer_size; i++)
				buffer[i] = GetDynamicData();
		}
		inline std::vector<std::string> GetStringArray(){
			std::vector<std::string> res(GetStringArraySize());
			GetStringArray(res.data(), res.size());
			return res;
		}
	};

	/*
	template<typename aT = size_t> class C_Read {
	public:
		inline C_Read() {
			if (!std::is_unsigned<aT>())
				Internal::ErrorMessages::InvalidAdressType();
		}
		inline C_Read(const Template& templ, const std::string& data) {
			if (!std::is_unsigned<aT>())
				Internal::ErrorMessages::InvalidAdressType();
			Init(templ, data);
		}
		inline C_Read(const Template& templ) {
			if (!std::is_unsigned<aT>())
				Internal::ErrorMessages::InvalidAdressType();
			Init(templ);
		}

		inline void Init(const Template& templ) {
			main_template = templ;
			cached_type_positions.resize(main_template.AmountOfTypes());
			data.clear();
		}
		inline void Init(const Template& templ, const std::string& data) {
			main_template = templ;
			this->data = data;

			if (data.size() >= max_size)
				Internal::Err("JSERIAL ERROR: the source string size is larger than the max value of the given adress type (" + std::to_string(data.size()) + " when the max size of the " + std::string(typeid(aT).name()) + " datatype is " + std::to_string(max_size) + ")");

			cached_type_positions.resize(main_template.AmountOfTypes());
			CachePositions();
		}

		inline void Source(const std::string& data) {
			if (main_template.AmountOfTypes()) {
				this->data = data;

				CachePositions();
			} else 
				Internal::Err("JSERIAL ERROR: empty template");
		}

		template<typename T> T ReadType(const std::string& key) {
			const aT curr_type_id = main_template.TypeId(key);
			const Internal::Type& curr_type = main_template[curr_type_id];

			if (curr_type.type_name == typeid(T).name() && !curr_type.is_array)
				return Bits::Merge<T>(data.substr(cached_type_positions[curr_type_id], curr_type.size));
			Internal::ErrorMessages::TypeMismatch({ "", 0, false, typeid(T).name() }, curr_type);
		}
		template<> std::string ReadType<std::string>(const std::string& key) {
			const aT curr_type_id = main_template.TypeId(key);
			const Internal::Type& curr_type = main_template[curr_type_id];

			if (curr_type.type_name == typeid(std::string).name() && !curr_type.is_array) {
				const aT start_pos = cached_type_positions[curr_type_id] + sizeof(aT);
				const aT string_size = Bits::Merge<aT>(data.substr(start_pos, sizeof(aT)));

				return data.substr(start_pos + sizeof(aT), string_size);
			}
			Internal::ErrorMessages::TypeMismatch({ "", 0, false, typeid(std::string).name() }, curr_type);
		}

		template<typename T> std::vector<T> ReadArray(const std::string& key) {
			const aT curr_type_id = main_template.TypeId(key);
			const Internal::Type& curr_type = main_template[curr_type_id];

			if (curr_type.type_name == typeid(T).name() && curr_type.is_array) {
				const aT start_pos = cached_type_positions[curr_type_id] + (2 * sizeof(aT));
				const aT arr_size = Bits::Merge<aT>(data.substr(start_pos - sizeof(aT), sizeof(aT)));

				std::vector<T> res(arr_size);
				memcpy(res.data(), &data[start_pos], arr_size * curr_type.size);
				return res;
			}
			Internal::ErrorMessages::TypeMismatch({ "", 0, true, typeid(T).name() }, curr_type);
		}
		template<> std::vector<std::string> ReadArray<std::string>(const std::string& key) {
			const aT curr_type_id = main_template.TypeId(key);
			const Internal::Type& curr_type = main_template[curr_type_id];

			if (curr_type.type_name == typeid(std::string).name() && curr_type.is_array) {
				aT cursor_pos = cached_type_positions[curr_type_id] + sizeof(aT);
				const aT arr_size = Bits::Merge<aT>(data.substr(cursor_pos, sizeof(aT)));
				cursor_pos += sizeof(size_t);

				std::vector<std::string> res(arr_size);
				cursor_pos += arr_size * sizeof(aT);

				for (aT i = 0; i < arr_size; i++) {
					const aT str_size = Bits::Merge<aT>(data.substr(cursor_pos, sizeof(aT)));
					cursor_pos += sizeof(aT);

					res[i] = data.substr(cursor_pos, str_size);
					cursor_pos += str_size;
				} return res;
			} 
			Internal::ErrorMessages::TypeMismatch({ "", 0, true, typeid(std::string).name() }, curr_type);
		}

		template<typename T> void ReadArray(const std::string& key, T* arr) {
			const aT curr_type_id = main_template.TypeId(key);
			const Internal::Type& curr_type = main_template[curr_type_id];

			if (curr_type.type_name == typeid(T).name() && curr_type.is_array) {
				const aT start_pos = cached_type_positions[curr_type_id] + (2 * sizeof(aT));
				const aT arr_size = Bits::Merge<aT>(data.substr(start_pos - sizeof(aT), sizeof(aT)));

				memcpy(arr, &data[start_pos], arr_size * curr_type.size);
			} else
			Internal::ErrorMessages::TypeMismatch({ "", 0, true, typeid(T).name() }, curr_type);
		}
		template<> void ReadArray<std::string>(const std::string& key, std::string* arr) {
			const aT curr_type_id = main_template.TypeId(key);
			const Internal::Type& curr_type = main_template[curr_type_id];

			if (curr_type.type_name == typeid(std::string).name() && curr_type.is_array) {
				aT cursor_pos = cached_type_positions[curr_type_id] + sizeof(aT);
				const aT arr_size = Bits::Merge<aT>(data.substr(cursor_pos - sizeof(aT), sizeof(aT)));
				cursor_pos += sizeof(size_t);

				cursor_pos += arr_size * sizeof(aT);

				for (aT i = 0; i < arr_size; i++) {
					const aT str_size = Bits::Merge<aT>(data.substr(cursor_pos, sizeof(aT)));
					cursor_pos += sizeof(aT);

					arr[i] = data.substr(cursor_pos, str_size);
					cursor_pos += str_size;
				}
			} else
			Internal::ErrorMessages::TypeMismatch({ "", 0, true, typeid(std::string).name() }, curr_type);
		}

		inline aT GetArraySize(const std::string& key) {
			const aT curr_type_id = main_template.TypeId(key);
			const Internal::Type& curr_type = main_template[curr_type_id];

			if (curr_type.is_array)
				return Bits::Merge<aT>(data.substr(cached_type_positions[curr_type_id] + sizeof(aT), sizeof(aT)));
			Internal::ErrorMessages::NotAnArray();
		}

		template<typename T> T ReadArrayElement(const std::string& key, aT id) {
			const aT curr_type_id = main_template.TypeId(key);
			const Internal::Type& curr_type = main_template[curr_type_id];

			if (curr_type.type_name == typeid(T).name() && curr_type.is_array) {
				const aT start_pos = cached_type_positions[curr_type_id] + sizeof(aT);
				const aT arr_size = Bits::Merge<aT>(data.substr(start_pos, sizeof(aT)));

				if (id < arr_size)
					return Bits::Merge<T>(data.substr(start_pos + sizeof(aT) + curr_type.size * id, curr_type.size));

				Internal::ErrorMessages::OutOfBounds(id, arr_size);
			} Internal::ErrorMessages::TypeMismatch({ "", 0, true, typeid(T).name() }, curr_type);
		}
		template<> std::string ReadArrayElement<std::string>(const std::string& key, aT id) {
			const aT curr_type_id = main_template.TypeId(key);
			const Internal::Type& curr_type = main_template[curr_type_id];

			if (curr_type.type_name == typeid(std::string).name() && curr_type.is_array) {
				const aT start_pos = cached_type_positions[curr_type_id] + sizeof(aT);
				const aT arr_size = Bits::Merge<aT>(data.substr(start_pos, sizeof(aT)));

				if (id < arr_size) {
					const aT string_start_pos = Bits::Merge<aT>(data.substr(start_pos + sizeof(aT) * (id + 1)));
					return data.substr(string_start_pos + sizeof(aT),  Bits::Merge<aT>(data.substr(string_start_pos, sizeof(aT))));
				} 
				Internal::ErrorMessages::OutOfBounds(id, arr_size);
			} Internal::ErrorMessages::TypeMismatch({ "", 0, true, typeid(std::string).name() }, curr_type);
		}
	private:
		static constexpr aT max_size = (0 - 1);
		Template main_template;
		std::string data;
		std::vector<aT> cached_type_positions;

		void CachePositions() {
			aT cursor_pos = 0;
			for (int i = 0; i < main_template.AmountOfTypes(); i++) {
				cached_type_positions[i] = cursor_pos;
				const Internal::Type& curr_type = main_template[i];

				if (!curr_type.is_array && curr_type.type_name != typeid(std::string).name())
					cursor_pos += curr_type.size;
				else
					cursor_pos = Bits::Merge<aT>(this->data.substr(cursor_pos, sizeof(aT)));
			}
		}
	};*/

	using Read = C_Read<size_t>;
	using Write = C_Write<size_t>;
}