#pragma once

#include "../../pch.h"

#include "ScriptFunction.hpp"
#include "Parser.hpp"

namespace gstd {
	class script_type_manager {
		static script_type_manager* base_;
	public:
		script_type_manager();

		static type_data* get_int_type() { return base_->int_type; }
		static type_data* get_real_type() { return base_->real_type; }
		static type_data* get_char_type() { return base_->char_type; }
		static type_data* get_boolean_type() { return base_->boolean_type; }
		
		static type_data* get_null_array_type() { return base_->null_array_type; }
		static type_data* get_string_type() { return base_->string_type; }
		static type_data* get_int_array_type() { return base_->int_array_type; }
		static type_data* get_real_array_type() { return base_->real_array_type; }

		type_data* get_array_type(type_data* element);

		static script_type_manager* get_instance() { return base_; }
	private:
		script_type_manager(const script_type_manager& src);

		std::set<type_data> types;

		//Common types for quick access without std::set traversal
		type_data* int_type;
		type_data* real_type;
		type_data* char_type;
		type_data* boolean_type;

		type_data* null_array_type;
		type_data* string_type;
		type_data* int_array_type;
		type_data* real_array_type;

		inline static type_data* deref_itr(std::set<type_data>::iterator& itr) {
			return const_cast<type_data*>(&*itr);
		}
	};

	class script_engine {
	public:
		script_engine(const std::string& source, std::vector<function>* list_func, std::vector<constant>* list_const);
		script_engine(const std::vector<char>& source, std::vector<function>* list_func, std::vector<constant>* list_const);
		virtual ~script_engine();

		script_engine& operator=(const script_engine& source) = default;

		bool get_error() { return error; }
		std::wstring& get_error_message() { return error_message; }
		int get_error_line() { return error_line; }

		script_block* new_block(int level, block_kind kind);
	public:
		void* data;		//Client script pointer

		bool error;
		std::wstring error_message;
		int error_line;

		std::list<script_block> blocks;
		script_block* main_block;
		std::map<std::string, script_block*> events;
	};

	class script_machine {
	public:
		script_machine(script_engine* the_engine);
		virtual ~script_machine();

		void* data;		//Pointer to client script class

		void run();
		void call(const std::string& event_name);
		void call(std::map<std::string, script_block*>::iterator event_itr);
		void resume();
		void stop() {
			finished = true;
			stopped = true;
		}

		bool get_stopped() { return stopped; }
		bool get_resuming() { return resuming; }

		bool get_error() { return error; }
		std::wstring& get_error_message() { return error_message; }
		int get_error_line() { return error_line; }

		void raise_error(const std::wstring& message) {
			error = true;
			error_message = message;
			finished = true;
		}
		void raise_error(const std::string& message) {
			error = true;
			error_message = StringUtility::ConvertMultiToWide(message);
			finished = true;
		}
		void terminate(const std::wstring& message) {
			bTerminate = true;
			raise_error(message);
		}

		script_engine* get_engine() { return engine; }

		bool has_event(const std::string& event_name, std::map<std::string, script_block*>::iterator& res);
		int get_current_line();
		int get_current_thread_addr() { return (int)current_thread_index._Ptr; }
	private:
		script_machine();
		script_machine(const script_machine& source);
		script_machine& operator=(const script_machine& source);

		script_engine* engine;

		bool error;
		std::wstring error_message;
		int error_line;

		bool bTerminate;

		typedef script_vector<value> variables_t;
		typedef script_vector<value> stack_t;

		class environment {
		public:
			ref_unsync_ptr<environment> parent;
			script_block* sub;
			int ip;
			variables_t variables;
			stack_t stack;
			bool has_result;
			int waitCount;
		public:
			environment(ref_unsync_ptr<environment> parent, script_block* b);
			~environment();
		};

		std::list<ref_unsync_ptr<environment>> list_parent_environment;

		std::list<ref_unsync_ptr<environment>> threads;
		std::list<ref_unsync_ptr<environment>>::iterator current_thread_index;
		bool finished;
		bool stopped;
		bool resuming;

		void yield() {
			if (current_thread_index == threads.begin())
				current_thread_index = std::prev(threads.end());
			else
				--current_thread_index;
		}

		void run_code();
		value* find_variable_symbol(environment* current_env, 
			code* c, uint32_t level, uint32_t variable, bool allow_null);
	public:
		size_t get_thread_count() { return threads.size(); }
	};
}
