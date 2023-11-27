#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <typeinfo>
#include <string.h>

using namespace std;

#define manual_write

namespace bits {
	template<typename T> string Separate(T dat) {
		return string(reinterpret_cast<char const*>(&dat), sizeof(T));
	}

	template<typename T> T Merge(string dat) {
		T a;
		memcpy(&a, dat.c_str(), sizeof(T));
		return a;
	}
}

namespace JSerial {
	struct JsType {
		string key;
		int size;
		bool is_array, is_pod;
		int template_link = -1;
		string t_name;
	};

	class JsTemplate {
	public:
		vector<JsType> types;
		vector<JsTemplate> templates;
		size_t starting_point = 0, start_pos = 0;
		unordered_map<string, size_t> typemap;

		int item_id = 0;
		template<typename T>
		void add_basic_type(string key) {
			types.push_back({ key, sizeof(T), false, is_pod<T>(), -1, typeid(T).name() });
			typemap[key] = types.size() - 1;
		}

		template<typename T>
		void add_array(string key) {
			types.push_back({ key, sizeof(T), true, is_pod<T>(), -1, typeid(T).name() });
			typemap[key] = types.size() - 1;
		}

		void add_template(string key, JsTemplate templ, bool is_arr) {
			templates.push_back(templ);
			JsType tmp = { key, 0, is_arr, false, templates.size() - 1, "templ" };
			types.push_back(tmp);
			typemap[key] = types.size() - 1;
		}
	};

	struct active_template_array {
		size_t number_of_elements = 0, layer = 0, current_element = 0, start_pos = 0;
	};

	void override_string(string* s1, string s2, size_t pos) {
		for (size_t i = 0; i < s2.size(); i++)
			s1->operator[](pos + i) = s2[i];
	}


	class JSerial {
	public:
		vector<JsTemplate> active_templates;
		JsTemplate main_template;
		string data;
		bool no_next_after_reading = 0;

		void init_write() {
			active_templates.push_back(main_template);
			active_templates[0].starting_point = 0;
		}

		//не поддерживает строки. Используйте функцию write_string()
		template<typename T> void write_basic_data(T dat) {
			JsType curr_type = get_current_type();
			if (typeid(T).name() == curr_type.t_name && curr_type.is_pod && !curr_type.is_array) {
				data += bits::Separate<T>(dat);
				cursor_pos += sizeof(T);
				check_and_switch();
			}
			else cout << "\nincorrect type bruh: " << typeid(T).name << " instead of " << curr_type.t_name << endl;
		}

		//не поддерживает строки. Используйте функцию write_string_array();
		template<typename T> void write_basic_array(vector<T> dat) {
			JsType curr_type = get_current_type();
			if (typeid(T).name() == curr_type.t_name && curr_type.is_pod && curr_type.is_array) {
				if (active_templates.back().item_id < active_templates.back().types.size() - 1)
					reserve_adresses(1);
				data += bits::Separate<size_t>(dat.size());
				cursor_pos += sizeof(size_t);
				for (T& i : dat)
					data += bits::Separate<T>(i);
				cursor_pos += sizeof(T) * dat.size();
				if (active_templates.back().item_id < active_templates.back().types.size() - 1)
					write_jmpt();
				check_and_switch();
			}
			else cout << "\nincorrect type bruh: " << typeid(T).name << " array" << " instead of " << curr_type.t_name << endl;
		}

		void write_string(string str) {
			JsType curr_type = get_current_type();
			if (curr_type.t_name == typeid(string).name() && !curr_type.is_array) {
				if (active_templates.back().item_id < active_templates.back().types.size() - 1)
					reserve_adresses(1);
				data += bits::Separate<size_t>(str.size());
				cursor_pos += sizeof(size_t);
				data += str;
				cursor_pos += str.size();
				if (active_templates.back().item_id < active_templates.back().types.size() - 1)
					write_jmpt();
				check_and_switch();
			}
			else cout << "\nincorrect type bruh: " << "string" << " instead of " << curr_type.t_name;
		}

		void write_string_array(vector<string> dat) {
			JsType curr_type = get_current_type();
			if (curr_type.t_name == typeid(string).name() && curr_type.is_array) {
				if (active_templates.back().item_id < active_templates.back().types.size() - 1)
					reserve_adresses(1);
				data += bits::Separate<size_t>(dat.size());
				cursor_pos += sizeof(size_t);
				for (string& i : dat)
					data += bits::Separate<size_t>(i.size()) + i, cursor_pos += sizeof(size_t) + i.size();
				if (active_templates.back().item_id < active_templates.back().types.size() - 1)
					write_jmpt();
				check_and_switch();
			}
			else cout << "\nincorrect type bruh: " << "string array" << " instead of " << curr_type.t_name << endl;
		}

		void prepare_template_array_write(size_t amount) {
			JsType curr_type = get_current_type();
			if (curr_type.t_name == "templ" && curr_type.is_array) {
				if (active_templates.back().item_id < active_templates.back().types.size() - 1)
					reserve_adresses(1);
				data += bits::Separate<size_t>(amount);
				cursor_pos += sizeof(size_t);
				reserve_adresses(amount);
				write_jmpt();
				active_templates.push_back(active_templates.back().templates[curr_type.template_link]);
				act_templ_arrs.push_back({ amount, active_templates.size(), 0, 0 /*стартовая позиция нужна только при чтении*/ });
#ifndef manual_write
				if (active_templates.back().types[0].t_name == "templ" && !active_templates.back().types[0].is_array)
					prepare_template_write();
#endif
			}
			else cout << "\nincorrect type bruh: " << "templ array" << " instead of " << curr_type.t_name << endl;
		}

		#ifdef manual_write

		#endif


		//--------------------------------------------------------------------------Чтение файла--------------------------------------------

		void init_read() {
			active_templates.clear();
			active_templates.push_back(main_template);
			cursor_pos = 0;
		}

		void next() {
			JsType curr_type = get_current_type();
			if (active_templates.size()) {
				if (active_templates.back().item_id < active_templates.back().types.size() - 1) {
					if (!curr_type.is_array && curr_type.is_pod)
						cursor_pos += curr_type.size;
					else cursor_pos = bits::Merge<size_t>(data.substr(cursor_pos, sizeof(size_t)));
					active_templates.back().item_id++;
				}
				else {
					if (is_template_array()) {
						if (act_templ_arrs.back().current_element < act_templ_arrs.back().number_of_elements - 1) {
							active_templates.back().item_id = 0, act_templ_arrs.back().current_element++;
							acess_element_of_template_array(act_templ_arrs.back().current_element);
						}
						else return leave_template_arr();
					}
					else return leave_template();
				}
			}
			else cout << "\nincorrect type bruh\n";
		}

		size_t enter_template_array() {
			JsType curr_type = get_current_type();
			if (!curr_type.is_pod && curr_type.is_array && curr_type.t_name == "templ") {

				act_templ_arrs.push_back({ 0, active_templates.size(), 0, cursor_pos });

				if (active_templates.back().item_id < active_templates.back().types.size() - 1)
					cursor_pos += sizeof(size_t);
				act_templ_arrs.back().number_of_elements = bits::Merge<size_t>(data.substr(cursor_pos, sizeof(size_t)));
				active_templates.push_back(active_templates.back().templates[curr_type.template_link]);
				acess_element_of_template_array(0);
				return act_templ_arrs.back().number_of_elements;
			}
			else cout << "\nincorrect type bruh\n";
		}

		//для работы этой функции необходимо войти в шаблонный массив. Функция также будет работать, если находишься в каком-либо из элементов
		void acess_element_of_template_array(size_t id) {
			if (act_templ_arrs.size()) {
				while (act_templ_arrs.back().layer < active_templates.size() - 1)
					active_templates.pop_back(), tmp_cursor_pos.pop_back();

				cursor_pos = act_templ_arrs.back().start_pos;

				if (active_templates[active_templates.size() - 2].item_id < active_templates[active_templates.size() - 2].types.size() - 1)
					cursor_pos += sizeof(size_t);
				cursor_pos += sizeof(size_t);

				cursor_pos += (id * sizeof(size_t));
				cursor_pos = bits::Merge<size_t>(data.substr(cursor_pos, sizeof(size_t)));

				act_templ_arrs.back().current_element = id;
			}
			else cout << "\nincorrect type bruh\n";
		}

		//выйти из шаблонного массива. Будет работать, даже если до массива есть несколько уровней обычных шаблонов.
		void leave_template_arr() {
			if (act_templ_arrs.size()) {
				cursor_pos = act_templ_arrs.back().start_pos;

				while (act_templ_arrs.back().layer <= active_templates.size() - 1)
					active_templates.pop_back();

				act_templ_arrs.pop_back();
			}
			if (!no_next_after_reading)
				next();
		}

		//войти в шаблон
		void enter_template() {
			JsType curr_type = get_current_type();
			if (is_template(curr_type)) {
				active_templates.push_back(active_templates.back().templates[get_current_type().template_link]);
				active_templates.back().start_pos = cursor_pos;

				if (active_templates[active_templates.size() - 2].item_id < active_templates[active_templates.size() - 2].types.size() - 1)
					cursor_pos += sizeof(size_t);
				active_templates.back().starting_point = cursor_pos;
			}
			else cout << "\nincorrect type bruh\n";
		}

		void leave_template() {
			if (active_templates.size() - 1) { //не выходить из главного шаблона
				JsType curr_type = get_current_type(2);
				if (is_template(curr_type))
					cursor_pos = active_templates.back().start_pos, active_templates.pop_back();
				if (!no_next_after_reading)
					next();
			}
			//else cout << "\nincorrect type bruh\n";
		}

		void go_to(string key) {
			int id = active_templates.back().typemap[key];
			if (id <= active_templates.back().types.size() - 1) {
				active_templates.back().item_id = 0;
				cursor_pos = active_templates.back().starting_point;
				for (int i = 0; i < id; i++) {
					JsType curr_type = get_current_type();
					if (!curr_type.is_array && curr_type.is_pod)
						cursor_pos += curr_type.size;
					else
						cursor_pos = bits::Merge<size_t>(data.substr(cursor_pos, sizeof(size_t)));
					active_templates.back().item_id++;
				}
			}
		}

		template<typename T> T read_basic_data() {
			JsType curr_type = get_current_type();
			if (curr_type.t_name == typeid(T).name()) {
				T a;
				a = bits::Merge<T>(data.substr(cursor_pos, curr_type.size));
				if (!no_next_after_reading)
					next();
				return a;
			}
			else cout << "\nincorrect type bruh\n";
		}

		template<typename T> vector<T> read_basic_array() {
			JsType curr_type = get_current_type();
			if (curr_type.t_name == typeid(T).name() && curr_type.is_pod) {
				vector<T> a;
				tmp_cursor_pos.push_back(cursor_pos);//необходимо для функции next()

				if (active_templates.back().item_id < active_templates.back().types.size() - 1)
					cursor_pos += sizeof(size_t);
				size_t arr_size = bits::Merge<size_t>(data.substr(cursor_pos, sizeof(size_t)));

				cursor_pos += sizeof(size_t);
				for (size_t i = 0; i < arr_size; i++) {
					string tmp = data.substr(cursor_pos, sizeof(T));
					cursor_pos += sizeof(T);
					a.push_back(bits::Merge<T>(tmp));
				}
				cursor_pos = tmp_cursor_pos.back();
				tmp_cursor_pos.pop_back();
				if (!no_next_after_reading)
					next();
				return a;
			}
			else cout << "\nincorrect type bruh\n";
		}

		string read_string() {
			JsType curr_type = get_current_type();
			if (curr_type.t_name == typeid(string).name()) {
				tmp_cursor_pos.push_back(cursor_pos);//необходимо для функции next()

				if (active_templates.back().item_id < active_templates.back().types.size() - 1)
					cursor_pos += sizeof(size_t);
				size_t strsize = bits::Merge<size_t>(data.substr(cursor_pos, sizeof(size_t)));

				cursor_pos += sizeof(size_t);
				string a = data.substr(cursor_pos, strsize);
				cursor_pos = tmp_cursor_pos.back();
				tmp_cursor_pos.pop_back();
				if (!no_next_after_reading)
					next();
				return a;
			}
			else cout << "\nincorrect type bruh\n";
		}

		vector<string> read_string_array() {
			JsType curr_type = get_current_type();
			if (curr_type.t_name == typeid(string).name()) {
				vector<string> a;
				tmp_cursor_pos.push_back(cursor_pos);//необходимо для функции next()

				if (active_templates.back().item_id < active_templates.back().types.size() - 1)
					cursor_pos += sizeof(size_t);
				size_t arr_size = bits::Merge<size_t>(data.substr(cursor_pos, sizeof(size_t)));

				cursor_pos += sizeof(size_t);
				for (size_t i = 0; i < arr_size; i++) {
					size_t str_size = bits::Merge<size_t>(data.substr(cursor_pos, sizeof(size_t)));
					cursor_pos += sizeof(size_t);
					a.push_back(data.substr(cursor_pos, str_size));
					cursor_pos += str_size;
				}
				cursor_pos = tmp_cursor_pos.back();
				tmp_cursor_pos.pop_back();
				if (!no_next_after_reading)
					next();
				return a;
			}
			else cout << "\nincorrect type bruh\n";
		}

	private:
		size_t cursor_pos = 0;
		vector<active_template_array> act_templ_arrs;
		vector<size_t> tmp_cursor_pos;

		inline bool is_template(JsType t) {
			return t.t_name == "templ" && !t.is_array && !t.is_pod;
		}

		JsType get_current_type(size_t level = 1) {
			if (level - 1 < active_templates.size())
				return active_templates[active_templates.size() - level].types[active_templates[active_templates.size() - level].item_id];
		}

		void write_jmpt() {
			override_string(&data, bits::Separate<size_t>(cursor_pos), tmp_cursor_pos.back());
			tmp_cursor_pos.pop_back();
		}

		void check_and_switch() {
			if (active_templates.back().item_id < active_templates.back().types.size() - 1) {
				active_templates.back().item_id++;
#ifndef manual_write
				if (!get_current_type().is_pod && !get_current_type().is_array && get_current_type().t_name != typeid(string).name())
					prepare_template_write();
#endif		
			}
			else {
				if (active_templates.size() == 1) return;
#ifndef manual_write
				if (!get_current_type(2).is_pod && !get_current_type(2).is_array)
					return end_template_write();
#endif		
				if (act_templ_arrs.size() && act_templ_arrs.back().layer == active_templates.size()) {
					act_templ_arrs.back().current_element++, active_templates.back().item_id = 0;

					if (act_templ_arrs.back().current_element < act_templ_arrs.back().number_of_elements) {
						write_jmpt();
#ifndef manual_write
						if (is_template(get_current_type()))
							prepare_template_write();
#endif
					}
#ifndef manual_write
					else end_template_array_write();
#endif
				}
			}
		}

		void reserve_adresses(size_t amount) {
			for (size_t i = amount; i > 0; i--) data += u8"        ", tmp_cursor_pos.push_back(cursor_pos + (i - 1) * sizeof(size_t));
			cursor_pos += amount * sizeof(size_t);
		}

#ifndef manual_write
		void prepare_template_write() {
			if (active_templates.back().item_id < active_templates.back().types.size() - 1)
				reserve_adresses(1);
			active_templates.push_back(active_templates.back().templates[get_current_type().template_link]);

			if (is_template(active_templates.back().types[0])) {
				prepare_template_write();
			}
		}

		void end_template_write() {
			if (active_templates[active_templates.size() - 2].item_id < active_templates[active_templates.size() - 2].types.size() - 1)
				write_jmpt();
			active_templates.pop_back();
			check_and_switch();
		}

		void end_template_array_write() {
			active_templates.pop_back();
			act_templ_arrs.pop_back();
			if (active_templates.back().item_id < active_templates.back().types.size() - 1)
				write_jmpt();
			check_and_switch();
}
		#endif
		//--------------------------------------------------------------------------Чтение файла--------------------------------------------

		bool is_template_array() {
			if (act_templ_arrs.size()) {
				return act_templ_arrs.back().layer == active_templates.size() - 1;
			}
		}
	};
}



/*
size_t get_array_size() {
	size_t tmp = cursor_pos;
	JsType curr_type = get_current_type();
	if (curr_type.is_array || curr_type.t_name == typeid(string).name()) {
		if (active_templates.back().item_id < active_templates.back().types.size() - 1 && curr_type.is_array)
			tmp += sizeof(size_t);
		cursor_pos = tmp;
		return bits::Merge<size_t>(data.substr(tmp, sizeof(size_t)));
	}
}*/
/*
inline size_t get_string_size() {
	size_t tmp = cursor_pos;
	JsType curr_type = get_current_type();
	if (curr_type.t_name == typeid(string).name()) {
		if (active_templates.back().item_id < active_templates.back().types.size() - 1 && !curr_type.is_array)
			tmp += sizeof(size_t);
		cursor_pos = tmp;
		return bits::Merge<size_t>(data.substr(tmp, sizeof(size_t)));
	}
}
*/
//inline size_t get_adress_by_cursor_pos() {
	//return bits::Merge<size_t>(data.substr(cursor_pos, sizeof(size_t)));
//}

/*
template<typename T> vector<T> read_basic_array() {
	JsTypes arr_type = active_templates.back().array_types[get_current_type().link_id];
	vector<T> a;
	JsTypesEnum curr_type = get_type<T>();
	if (curr_type == arr_type.type) {
		long long arr_size = get_array_size();
		cursor_pos += size_table[js_long_long];
		for (long long i = 0; i < arr_size; i++) {
			string tmp = read_str(cursor_pos, size_table[curr_type]);
			cursor_pos += size_table[curr_type];
			a.push_back(bits::MergeG<T>(tmp));
		}
	}
	next();
	return a;
}*/

/*
vector<int> read_int_array() {
	vector<int> a;
	tmp_cursor_pos.push_back(cursor_pos);
	int array_size = get_array_size();
	a.resize(array_size);
	cursor_pos += size_table[js_int];
	for (int i = 0; i < array_size; i++) {
		a[i] = get_int_value(cursor_pos);
		cursor_pos += size_table[js_int];
	}
	cursor_pos = tmp_cursor_pos.back();
	tmp_cursor_pos.pop_back();
	next();
	return a;
}*/

/*
void go_to(int id) {
	if (id <= active_templates.back().types.size() - 1) {
		active_templates.back().item_id = 0;
		cursor_pos = active_templates.back().starting_point;
		for (int i = 0; i < id; i++) {
			JsType curr_type = get_current_type();
			if (!curr_type.is_array && curr_type.is_pod)
				cursor_pos += curr_type.size;
			else {
				cursor_pos = bits::Merge<size_t>(data.substr(cursor_pos, sizeof(size_t)));
			}
			active_templates.back().item_id++;
		}
	}
}
*/

/*
	void write_string(string a) {
		if (get_current_type().type == js_string) {
			data += bits::SeparateLL(a.size());
			cursor_pos += size_table[js_long_long];
			data += a;
			cursor_pos += a.size();
			check_and_switch();
		}
	}*/
	/*
	void prepare_template_array_write(int amount) {
		if (active_templates.back().find_arr().type == js_template) {
			if (active_templates.back().item_id < active_templates.back().types.size() - 1)
				reserve_adresses(1);
			data += bits::Separate(amount);
			cursor_pos += size_table[js_int];
			reserve_adresses(amount);
			write_jmpt();
			active_templates.push_back(*active_templates.back().find_template());
			act_templ_arrs.push_back({ int(active_templates.size()), amount, 0, 0 });
		}
	}*/