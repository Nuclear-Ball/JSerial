#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <codecvt>

namespace JSerial {
	namespace Utilities {
		template<typename T> inline std::string MemblockToString(T* memory, size_t size)
			{ return std::string(reinterpret_cast<const char*>(memory), sizeof(T) * size); }

		inline void OverrideString(std::string* s1, const std::string& s2, size_t pos)
			{ memcpy(&s1->operator[](pos), s2.data(), s2.size()); }

		std::string ReadFile(const std::string& path) {
			std::ifstream file(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(path), std::ios::binary);

			file.seekg(0, std::ios::end);
			const size_t filesize = file.tellg();
			file.seekg(0, std::ios::beg);

			constexpr size_t invalid_filesize = 18446744073709551615;
			if (filesize == invalid_filesize) {
				std::cerr << "ERROR: file was not successfully opened\n";
				return "";
			}

			std::string res(filesize, 0);

			file.read(const_cast<char*>(res.data()), filesize);
			file.close();

			return res;
		}
		void SaveFile(const std::string& path, const std::string& data) {
			std::ofstream file(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(path), std::ios::binary);
			file.write(data.c_str(), data.size());
			file.close();
		}
	}
	namespace Internal {
		inline void Err(const std::string& msg) {
			std::cerr << msg;

			#ifndef JSERIAL_DISABLE_ERROR_SHUTDOWN
			std::abort();
			#endif
		}

		struct Type {
			std::string key;
			int size;
			bool is_array;
			std::string type_name;
		};
		namespace ErrorMessages {
			inline void OutOfBounds(size_t curr_size, size_t max_size) 
				{ Err(std::string("JSERIAL ERROR: trying to access element of an array out of bounds: " + std::to_string(curr_size) + " when max size is " + std::to_string(max_size))); }
			inline void NotAnArray(const std::string& key) 
				{ Err("JSERIAL ERROR: type \42" + key + "\42 is not an array"); }
			inline void InvalidAdressType() 
				{ Err("JSERIAL ERROR: invalid adress type: must be an unsigned integer"); }
			inline void TypeMismatch(const Type& given_type, const Type& expected_type)
				{ Err("JSERIAL ERROR: type mismatch: " + given_type.type_name + (given_type.is_array ? " array" : "") + " instead of " + expected_type.type_name + (expected_type.is_array ? " array" : "") + ", the key for type is \52" + expected_type.key + '\52'); }
		}
		
	}
	namespace Bits {
		template<typename T> inline std::string Separate(T dat) 
			{ return std::string(reinterpret_cast<const char*>(&dat), sizeof(T)); }
		template<typename T> inline T Merge(const std::string& dat) 
			{ return *reinterpret_cast<const T*>(dat.data()); }
	}

	template<typename T> inline Internal::Type Type(const std::string& key) {
		if (!std::is_pod<T>() && !std::is_same<T, std::string>())
			Internal::Err("JSERIAL ERROR: unsupported type: only strings and PODs are supported");
		return { key, sizeof(T), false, typeid(T).name() };
	}
	template<typename T> inline Internal::Type Array(const std::string& key) {
		if (!std::is_pod<T>() && !std::is_same<T, std::string>())
			Internal::Err("JSERIAL ERROR: unsupported type: only strings and PODs are supported");
		return { key, sizeof(T), true, typeid(T).name() };
	}

	class Template {
	private:
		std::vector<Internal::Type> types;
		std::unordered_map<std::string, size_t> typemap;
	public:
		Template() {}
		Template(const std::initializer_list<Internal::Type>& lst) {
			types.resize(lst.size());
			for (int i = 0; i < lst.size(); i++) {
				types[i] = lst.begin()[i];
				typemap[types[i].key] = i + 1;
			}
		}

		template<typename T> void AddType(const std::string& key) {
			types.push_back(Type<T>(key));
			typemap[key] = types.size();
		}
		template<typename T> void AddArray(const std::string& key) {
			types.push_back(Array<T>(key));
			typemap[key] = types.size();
		}

		inline size_t AmountOfTypes() const
			{ return types.size(); }
		inline size_t TypeId(const std::string& key) {
			const size_t id = typemap[key];
			if (id)
				return id - 1;
			Internal::Err("JSERIAL ERROR: type with that key doesn't exists: " + key);
		}

		inline const Internal::Type& operator[](size_t id)
			{ return types[id]; }
		inline const Internal::Type& operator[](const std::string& key)
			{ return types[TypeId(key)]; }
	};

	template<typename aT = size_t> class C_Write {
	public:
		inline C_Write() {
			if (!std::is_unsigned<aT>())
				Internal::ErrorMessages::InvalidAdressType();
		}
		inline C_Write(const Template& templ) {
			if (!std::is_unsigned<aT>())
				Internal::ErrorMessages::InvalidAdressType();
			main_template = templ;
		}

		template<typename T> void WriteType(const T& dat) {
			const Internal::Type& curr_type = main_template[item_id];

			if (typeid(T).name() == curr_type.type_name && !curr_type.is_array) {
				data += Bits::Separate<T>(dat);
				cursor_pos += curr_type.size;
				item_id++;
			}
			else Internal::ErrorMessages::TypeMismatch({ "", 0, false, typeid(T).name() }, curr_type);
		}
		template<> void WriteType<std::string>(const std::string& dat) {
			const Internal::Type& curr_type = main_template[item_id];

			if (curr_type.type_name == typeid(std::string).name() && !curr_type.is_array) {
				ReserveAdresses(1);

				data += Bits::Separate<aT>(dat.size());
				cursor_pos += sizeof(aT);

				data += dat;
				cursor_pos += dat.size();

				WriteJmpt();
				item_id++;
			}
			else Internal::ErrorMessages::TypeMismatch({ "", 0, false, typeid(std::string).name() }, curr_type);
		}

		template<typename T> void WriteArray(const std::vector<T>& dat) {
			const Internal::Type& curr_type = main_template[item_id];

			if (typeid(T).name() == curr_type.type_name && curr_type.is_array) {
				ReserveAdresses(1);

				data += Bits::Separate<aT>(dat.size());
				cursor_pos += sizeof(aT);

				data.resize(data.size() + dat.size() * curr_type.size);
				memcpy(&data[cursor_pos], dat.data(), dat.size() * curr_type.size);
				cursor_pos += curr_type.size * dat.size();

				WriteJmpt();
				item_id++;
			}
			else Internal::ErrorMessages::TypeMismatch({ "", 0, true, typeid(T).name() }, curr_type);
		}
		template<> void WriteArray<std::string>(const std::vector<std::string>& dat) {
			const Internal::Type& curr_type = main_template[item_id];

			if (curr_type.type_name == typeid(std::string).name() && curr_type.is_array) {
				ReserveAdresses(1);

				data += Bits::Separate<aT>(dat.size());
				cursor_pos += sizeof(aT);

				ReserveAdresses(dat.size());

				for (const std::string& i : dat) {
					WriteJmpt();

					data += Bits::Separate<aT>(i.size()) + i;
					cursor_pos += sizeof(aT) + i.size();
				}

				WriteJmpt();
				item_id++;
			}
			else Internal::ErrorMessages::TypeMismatch({ "", 0, true, typeid(std::string).name() }, curr_type);
		}

		template<typename T> void WriteArray(const T* dat, aT size) {
			const Internal::Type& curr_type = main_template[item_id];

			if (typeid(T).name() == curr_type.type_name && curr_type.is_array) {
				ReserveAdresses(1);

				data += Bits::Separate<aT>(size);
				cursor_pos += sizeof(aT);

				data.resize(data.size() + size * curr_type.size);
				memcpy(&data[cursor_pos], dat, size * curr_type.size);
				cursor_pos += curr_type.size * dat.size();

				WriteJmpt();
				item_id++;
			}
			else Internal::ErrorMessages::TypeMismatch({ "", 0, true, typeid(T).name() }, curr_type);
		}
		template<> void WriteArray<std::string>(const std::string* dat, aT size) {
			const Internal::Type& curr_type = main_template[item_id];

			if (curr_type.type_name == typeid(std::string).name() && curr_type.is_array) {
				ReserveAdresses(1);

				data += Bits::Separate<aT>(size);
				cursor_pos += sizeof(aT);

				ReserveAdresses(size);

				for (aT i = 0; i < size; i++) {
					WriteJmpt();

					data += Bits::Separate<aT>(dat[i].size()) + dat[i];
					cursor_pos += sizeof(aT) + dat[i].size();
				}

				WriteJmpt();
				item_id++;
			}
			else Internal::ErrorMessages::TypeMismatch({ "", 0, true, typeid(std::string).name() }, curr_type);
		}

		inline void Reset() {
			cursor_pos = 0;
			item_id = 0;
			data.clear();
		}
		inline void Reset(const Template& templ) {
			cursor_pos = 0;
			item_id = 0;
			data.clear();

			main_template = templ;
		}

		inline std::string Result() {
			if (item_id == main_template.AmountOfTypes() && (data.size() <= max_size))
				return data;
			Internal::Err("JSERIAL ERROR: template was not fully filled(" + std::to_string(item_id == main_template.AmountOfTypes()) + ", 1 stands for filled and 0 for not filled) or an output string is larger than the max size for the given adress datatype (" + std::to_string(data.size()) + ", when the max size is: " + std::to_string(max_size) + ")");
		}

	private:
		static constexpr aT max_size = (0 - 1);
		Template main_template;
		std::string data;
		aT cursor_pos = 0;
		aT item_id = 0;
		std::list<aT> tmp_cursor_pos;

		inline void WriteJmpt() {
			Utilities::OverrideString(&data, Bits::Separate<aT>(cursor_pos), tmp_cursor_pos.back());
			tmp_cursor_pos.pop_back();
		}
		inline void ReserveAdresses(aT amount) {
			data.resize(data.size() + sizeof(aT) * amount);
			for (aT i = amount; i > 0; i--) 
				tmp_cursor_pos.push_back(cursor_pos + (i - 1) * sizeof(aT));
			cursor_pos += amount * sizeof(aT);
		}
	};
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
	};

	using Read = C_Read<size_t>;
	using Write = C_Write<size_t>;
}