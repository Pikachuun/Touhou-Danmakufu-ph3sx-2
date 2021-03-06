#pragma once

#include "../pch.h"

#include "GstdUtility.hpp"
#include "Script/Script.hpp"
#include "RandProvider.hpp"
#include "Thread.hpp"
#include "File.hpp"
#include "Logger.hpp"

namespace gstd {
	class ScriptFileLineMap;
	class ScriptCommonDataManager;

	/**********************************************************
	//ScriptEngineData
	**********************************************************/
	class ScriptEngineData {
	protected:
		std::wstring path_;

		Encoding::Type encoding_;
		std::vector<char> source_;

		ref_count_ptr<script_engine> engine_;
		ref_count_ptr<ScriptFileLineMap> mapLine_;
	public:
		ScriptEngineData();
		virtual ~ScriptEngineData();

		void SetPath(const std::wstring& path) { path_ = path; }
		std::wstring& GetPath() { return path_; }

		void SetSource(std::vector<char>& source);
		std::vector<char>& GetSource() { return source_; }
		Encoding::Type GetEncoding() { return encoding_; }

		void SetEngine(ref_count_ptr<script_engine>& engine) { engine_ = engine; }
		ref_count_ptr<script_engine> GetEngine() { return engine_; }

		ref_count_ptr<ScriptFileLineMap> GetScriptFileLineMap() { return mapLine_; }
		void SetScriptFileLineMap(ref_count_ptr<ScriptFileLineMap>& mapLine) { mapLine_ = mapLine; }
	};

	/**********************************************************
	//ScriptEngineCache
	**********************************************************/
	class ScriptEngineCache {
	protected:
		std::map<std::wstring, ref_count_ptr<ScriptEngineData>> cache_;
	public:
		ScriptEngineCache();
		virtual ~ScriptEngineCache();
		void Clear();

		void AddCache(const std::wstring& name, ref_count_ptr<ScriptEngineData>& data);
		ref_count_ptr<ScriptEngineData> GetCache(const std::wstring& name);

		bool IsExists(const std::wstring& name);
	};

	/**********************************************************
	//ScriptBase
	**********************************************************/
	class ScriptClientBase {
		static script_type_manager* pTypeManager_;
	public:
		enum {
			ID_SCRIPT_FREE = -1,
		};
	protected:
		bool bError_;

		ref_count_ptr<ScriptEngineCache> cache_;
		ref_count_ptr<ScriptEngineData> engine_;
		unique_ptr<script_machine> machine_;

		std::vector<gstd::function> func_;
		std::vector<gstd::constant> const_;
		shared_ptr<RandProvider> mt_;
		shared_ptr<RandProvider> mtEffect_;

		shared_ptr<ScriptCommonDataManager> commonDataManager_;

		gstd::CriticalSection criticalSection_;

		int mainThreadID_;
		int64_t idScript_;

		std::vector<gstd::value> listValueArg_;
		gstd::value valueRes_;
	protected:
		void _AddFunction(const char* name, callback f, size_t arguments);
		void _AddFunction(const std::vector<gstd::function>* f);
		void _AddConstant(const std::vector<gstd::constant>* c);

		void _RaiseErrorFromEngine();
		void _RaiseErrorFromMachine();
		void _RaiseError(int line, const std::wstring& message);
		std::wstring _GetErrorLineSource(int line);

		virtual std::vector<char> _Include(std::vector<char>& source);
		virtual bool _CreateEngine();

		std::wstring _ExtendPath(std::wstring path);
	public:
		ScriptClientBase();
		virtual ~ScriptClientBase();

		static script_type_manager* GetDefaultScriptTypeManager() { return pTypeManager_; }

		void SetScriptEngineCache(ref_count_ptr<ScriptEngineCache>& cache) { cache_ = cache; }

		ref_count_ptr<ScriptEngineData> GetEngine() { return engine_; }

		virtual bool SetSourceFromFile(std::wstring path);
		virtual void SetSource(std::vector<char>& source);
		virtual void SetSource(const std::string& source);

		std::wstring& GetPath() { return engine_->GetPath(); }
		void SetPath(const std::wstring& path) { engine_->SetPath(path); }

		virtual void Compile();
		virtual bool Run();
		virtual bool Run(const std::string& target);
		virtual bool Run(std::map<std::string, script_block*>::iterator target);
		bool IsEventExists(const std::string& name, std::map<std::string, script_block*>::iterator& res);
		void RaiseError(const std::wstring& error) { _RaiseError(machine_->get_error_line(), error); }
		void RaiseError(const std::string& error) {
			_RaiseError(machine_->get_error_line(), 
				StringUtility::ConvertMultiToWide(error));
		}
		void Terminate(const std::wstring& error) { machine_->terminate(error); }
		int64_t GetScriptID() { return idScript_; }
		size_t GetThreadCount();

		void AddArgumentValue(value v) { listValueArg_.push_back(v); }
		void SetArgumentValue(value v, int index = 0);
		value GetResultValue() { return valueRes_; }

		static inline value CreateRealValue(double r);
		static inline value CreateIntValue(int64_t r);
		static inline value CreateBooleanValue(bool b);
		static inline value CreateStringValue(const std::wstring& s);
		static inline value CreateStringValue(const std::string& s);
		template<typename T> static inline value CreateRealArrayValue(std::vector<T>& list);
		template<typename T> static value CreateRealArrayValue(T* ptrList, size_t count);
		template<typename T> static inline value CreateIntArrayValue(std::vector<T>& list);
		template<typename T> static value CreateIntArrayValue(T* ptrList, size_t count);
		static value CreateStringArrayValue(std::vector<std::string>& list);
		static value CreateStringArrayValue(std::vector<std::wstring>& list);
		value CreateValueArrayValue(std::vector<value>& list);
		static bool IsRealValue(value& v);
		static bool IsIntValue(value& v);
		static bool IsBooleanValue(value& v);
		static bool IsStringValue(value& v);
		static bool IsArrayValue(value& v);
		static bool IsArrayValue(value& v, type_data* element);
		static bool IsRealArrayValue(value& v);
		static bool IsIntArrayValue(value& v);

		static void IsMatrix(script_machine* machine, const value& v);
		static void IsVector(script_machine* machine, const value& v, size_t count);

		void CheckRunInMainThread();
		ScriptCommonDataManager* GetCommonDataManager() { return commonDataManager_.get(); }

		//共通関数：スクリプト引数結果
		static value Func_GetScriptArgument(script_machine* machine, int argc, const value* argv);
		static value Func_GetScriptArgumentCount(script_machine* machine, int argc, const value* argv);
		static value Func_SetScriptResult(script_machine* machine, int argc, const value* argv);

		//共通関数：数学系
		static value Func_Min(script_machine* machine, int argc, const value* argv);
		static value Func_Max(script_machine* machine, int argc, const value* argv);
		static value Func_Clamp(script_machine* machine, int argc, const value* argv);
		static value Func_Log(script_machine* machine, int argc, const value* argv);
		static value Func_Log10(script_machine* machine, int argc, const value* argv);

		static value Func_Cos(script_machine* machine, int argc, const value* argv);
		static value Func_Sin(script_machine* machine, int argc, const value* argv);
		static value Func_Tan(script_machine* machine, int argc, const value* argv);
		DNH_FUNCAPI_DECL_(Func_SinCos);
		DNH_FUNCAPI_DECL_(Func_RCos);
		DNH_FUNCAPI_DECL_(Func_RSin);
		DNH_FUNCAPI_DECL_(Func_RTan);
		DNH_FUNCAPI_DECL_(Func_RSinCos);

		static value Func_Acos(script_machine* machine, int argc, const value* argv);
		static value Func_Asin(script_machine* machine, int argc, const value* argv);
		static value Func_Atan(script_machine* machine, int argc, const value* argv);
		static value Func_Atan2(script_machine* machine, int argc, const value* argv);
		DNH_FUNCAPI_DECL_(Func_RAcos);
		DNH_FUNCAPI_DECL_(Func_RAsin);
		DNH_FUNCAPI_DECL_(Func_RAtan);
		DNH_FUNCAPI_DECL_(Func_RAtan2);

		DNH_FUNCAPI_DECL_(Func_Exp);
		DNH_FUNCAPI_DECL_(Func_Sqrt);
		DNH_FUNCAPI_DECL_(Func_NRoot);
		DNH_FUNCAPI_DECL_(Func_Hypot);

		static value Func_Rand(script_machine* machine, int argc, const value* argv);
		DNH_FUNCAPI_DECL_(Func_RandI);
		DNH_FUNCAPI_DECL_(Func_RandEff);
		DNH_FUNCAPI_DECL_(Func_RandEffI);

		DNH_FUNCAPI_DECL_(Func_ToDegrees);
		DNH_FUNCAPI_DECL_(Func_ToRadians);
		DNH_FUNCAPI_DECL_(Func_NormalizeAngle);
		DNH_FUNCAPI_DECL_(Func_RNormalizeAngle);

		DNH_FUNCAPI_DECL_(Func_Interpolate_Linear);
		DNH_FUNCAPI_DECL_(Func_Interpolate_Smooth);
		DNH_FUNCAPI_DECL_(Func_Interpolate_Smoother);
		DNH_FUNCAPI_DECL_(Func_Interpolate_Accelerate);
		DNH_FUNCAPI_DECL_(Func_Interpolate_Decelerate);
		DNH_FUNCAPI_DECL_(Func_Interpolate_Modulate);
		DNH_FUNCAPI_DECL_(Func_Interpolate_Overshoot);
		DNH_FUNCAPI_DECL_(Func_Interpolate_QuadraticBezier);
		DNH_FUNCAPI_DECL_(Func_Interpolate_CubicBezier);
		DNH_FUNCAPI_DECL_(Func_Interpolate_Hermite);

		//共通関数：文字列操作
		static value Func_ToString(script_machine* machine, int argc, const value* argv);
		static value Func_ItoA(script_machine* machine, int argc, const value* argv);
		static value Func_RtoA(script_machine* machine, int argc, const value* argv);
		DNH_FUNCAPI_DECL_(Func_RtoA_Ex);
		static value Func_RtoS(script_machine* machine, int argc, const value* argv);
		static value Func_VtoS(script_machine* machine, int argc, const value* argv);
		static value Func_AtoI(script_machine* machine, int argc, const value* argv);
		static value Func_AtoR(script_machine* machine, int argc, const value* argv);
		static value Func_TrimString(script_machine* machine, int argc, const value* argv);
		static value Func_SplitString(script_machine* machine, int argc, const value* argv);

		DNH_FUNCAPI_DECL_(Func_RegexMatch);
		DNH_FUNCAPI_DECL_(Func_RegexMatchRepeated);
		DNH_FUNCAPI_DECL_(Func_RegexReplace);

		//共通関数：パス関連
		static value Func_GetParentScriptDirectory(script_machine* machine, int argc, const value* argv);
		static value Func_GetCurrentScriptDirectory(script_machine* machine, int argc, const value* argv);
		static value Func_GetFilePathList(script_machine* machine, int argc, const value* argv);
		static value Func_GetDirectoryList(script_machine* machine, int argc, const value* argv);

		//Path utility
		DNH_FUNCAPI_DECL_(Func_GetModuleName);
		DNH_FUNCAPI_DECL_(Func_GetModuleDirectory);
		DNH_FUNCAPI_DECL_(Func_GetFileDirectory);
		DNH_FUNCAPI_DECL_(Func_GetFileDirectoryFromModule);
		DNH_FUNCAPI_DECL_(Func_GetFileTopDirectory);
		DNH_FUNCAPI_DECL_(Func_GetFileName);
		DNH_FUNCAPI_DECL_(Func_GetFileNameWithoutExtension);
		DNH_FUNCAPI_DECL_(Func_GetFileExtension);
		DNH_FUNCAPI_DECL_(Func_IsFileExists);
		DNH_FUNCAPI_DECL_(Func_IsDirectoryExists);

		//共通関数：時刻関連
		static value Func_GetCurrentDateTimeS(script_machine* machine, int argc, const value* argv);

		//共通関数：デバッグ関連
		static value Func_WriteLog(script_machine* machine, int argc, const value* argv);
		static value Func_RaiseError(script_machine* machine, int argc, const value* argv);
		DNH_FUNCAPI_DECL_(Func_RaiseMessageWindow);

		//共通関数：共通データ
		static value Func_SetCommonData(script_machine* machine, int argc, const value* argv);
		static value Func_GetCommonData(script_machine* machine, int argc, const value* argv);
		static value Func_ClearCommonData(script_machine* machine, int argc, const value* argv);
		static value Func_DeleteCommonData(script_machine* machine, int argc, const value* argv);
		static value Func_SetAreaCommonData(script_machine* machine, int argc, const value* argv);
		static value Func_GetAreaCommonData(script_machine* machine, int argc, const value* argv);
		static value Func_ClearAreaCommonData(script_machine* machine, int argc, const value* argv);
		static value Func_DeleteAreaCommonData(script_machine* machine, int argc, const value* argv);
		DNH_FUNCAPI_DECL_(Func_DeleteWholeAreaCommonData);
		static value Func_CreateCommonDataArea(script_machine* machine, int argc, const value* argv);
		static value Func_CopyCommonDataArea(script_machine* machine, int argc, const value* argv);
		static value Func_IsCommonDataAreaExists(script_machine* machine, int argc, const value* argv);
		static value Func_GetCommonDataAreaKeyList(script_machine* machine, int argc, const value* argv);
		static value Func_GetCommonDataValueKeyList(script_machine* machine, int argc, const value* argv);
	};
#pragma region ScriptClientBase_impl
	value ScriptClientBase::CreateRealValue(double r) {
		return value(script_type_manager::get_real_type(), r);
	}
	value ScriptClientBase::CreateIntValue(int64_t r) {
		return value(script_type_manager::get_int_type(), r);
	}
	value ScriptClientBase::CreateBooleanValue(bool b) {
		return value(script_type_manager::get_boolean_type(), b);
	}
	value ScriptClientBase::CreateStringValue(const std::wstring& s) {
		return value(script_type_manager::get_string_type(), s);
	}
	value ScriptClientBase::CreateStringValue(const std::string& s) {
		return CreateStringValue(StringUtility::ConvertMultiToWide(s));
	}
	template<typename T> value ScriptClientBase::CreateRealArrayValue(std::vector<T>& list) {
		return CreateRealArrayValue(list.data(), list.size());
	}
	template<typename T> value ScriptClientBase::CreateIntArrayValue(std::vector<T>& list) {
		return CreateIntArrayValue(list.data(), list.size());
	}
	template<typename T>
	value ScriptClientBase::CreateRealArrayValue(T* ptrList, size_t count) {
		type_data* type_real = script_type_manager::get_real_type();
		type_data* type_arr = script_type_manager::get_real_array_type();
		if (ptrList && count > 0) {
			std::vector<value> res_arr;
			res_arr.resize(count);
			for (size_t iVal = 0U; iVal < count; ++iVal) {
				res_arr[iVal] = value(type_real, (double)(ptrList[iVal]));
			}

			value res;
			res.reset(type_arr, res_arr);
			return res;
		}
		return value(type_arr, 0i64);
	}
	template<typename T>
	value ScriptClientBase::CreateIntArrayValue(T* ptrList, size_t count) {
		type_data* type_int = script_type_manager::get_int_type();
		type_data* type_arr = script_type_manager::get_int_array_type();
		if (ptrList && count > 0) {
			std::vector<value> res_arr;
			res_arr.resize(count);
			for (size_t iVal = 0U; iVal < count; ++iVal) {
				res_arr[iVal] = value(type_int, (int64_t)(ptrList[iVal]));
			}

			value res;
			res.reset(type_arr, res_arr);
			return res;
		}
		return value(type_arr, 0i64);
	}
#pragma endregion ScriptClientBase_impl

	/**********************************************************
	//ScriptFileLineMap
	**********************************************************/
	class ScriptFileLineMap {
	public:
		struct Entry {
			int lineStart_;
			int lineEnd_;
			int lineStartOriginal_;
			int lineEndOriginal_;
			std::wstring path_;
		};
	protected:
		std::list<Entry> listEntry_;
	public:
		ScriptFileLineMap();
		virtual ~ScriptFileLineMap();
		void AddEntry(const std::wstring& path, int lineAdd, int lineCount);
		Entry* GetEntry(int line);
		std::wstring& GetPath(int line);
		std::list<Entry>& GetEntryList() { return listEntry_; }
	};

	/**********************************************************
	//ScriptCommonData
	**********************************************************/
	class ScriptCommonData {
	public:
		static constexpr const char* HEADER_SAVED_DATA = "DNHCDR\0\0";
		static constexpr size_t HEADER_SAVED_DATA_SIZE = 8U;
	protected:
		std::map<std::string, gstd::value> mapValue_;

		gstd::value _ReadRecord(gstd::ByteBuffer& buffer);
		void _WriteRecord(gstd::ByteBuffer& buffer, const gstd::value& comValue);
	public:
		ScriptCommonData();
		virtual ~ScriptCommonData();

		void Clear();
		std::pair<bool, std::map<std::string, gstd::value>::iterator> IsExists(const std::string& name);

		gstd::value GetValue(const std::string& name);
		gstd::value GetValue(std::map<std::string, gstd::value>::iterator itr);
		void SetValue(const std::string& name, gstd::value v);
		void SetValue(std::map<std::string, gstd::value>::iterator itr, gstd::value v);
		void DeleteValue(const std::string& name);
		void Copy(shared_ptr<ScriptCommonData>& dataSrc);

		std::map<std::string, gstd::value>::iterator MapBegin() { return mapValue_.begin(); }
		std::map<std::string, gstd::value>::iterator MapEnd() { return mapValue_.end(); }

		void ReadRecord(gstd::RecordBuffer& record);
		void WriteRecord(gstd::RecordBuffer& record);
	};

	/**********************************************************
	//ScriptCommonDataManager
	**********************************************************/
	class ScriptCommonDataManager {
	public:
		using CommonDataMap = std::map<std::string, shared_ptr<ScriptCommonData>>;
	protected:
		gstd::CriticalSection lock_;
		CommonDataMap mapData_;
		CommonDataMap::iterator defaultAreaIterator_;
	public:
		static const std::string nameAreaDefault_;

		ScriptCommonDataManager();
		virtual ~ScriptCommonDataManager();

		void Clear();
		void Erase(const std::string& name);

		const std::string& GetDefaultAreaName() { return nameAreaDefault_; }
		CommonDataMap::iterator GetDefaultAreaIterator() { return defaultAreaIterator_; }

		std::pair<bool, CommonDataMap::iterator> IsExists(const std::string& name);
		CommonDataMap::iterator CreateArea(const std::string& name);
		void CopyArea(const std::string& nameDest, const std::string& nameSrc);
		shared_ptr<ScriptCommonData> GetData(const std::string& name);
		shared_ptr<ScriptCommonData> GetData(CommonDataMap::iterator itr);
		void SetData(const std::string& name, shared_ptr<ScriptCommonData> commonData);
		void SetData(CommonDataMap::iterator itr, shared_ptr<ScriptCommonData> commonData);

		CommonDataMap::iterator MapBegin() { return mapData_.begin(); }
		CommonDataMap::iterator MapEnd() { return mapData_.end(); }

		gstd::CriticalSection& GetLock() { return lock_; }
	};

	/**********************************************************
	//ScriptCommonDataInfoPanel
	**********************************************************/
	class ScriptCommonDataInfoPanel : public WindowLogger::Panel {
	protected:
		enum {
			COL_AREA = 0,
			COL_KEY = 0,
			COL_VALUE,
		};

		shared_ptr<ScriptCommonDataManager> commonDataManager_;
		std::vector<std::map<std::string, shared_ptr<ScriptCommonData>>::iterator> vecMapItr_;

		gstd::CriticalSection lock_;

		WSplitter wndSplitter_;
		WListView wndListViewArea_;
		WListView wndListViewValue_;
		int timeLastUpdate_;
		int timeUpdateInterval_;

		virtual bool _AddedLogger(HWND hTab);

		void _UpdateAreaView();
		void _UpdateValueView();
	public:
		ScriptCommonDataInfoPanel();
		void SetUpdateInterval(int time) { timeUpdateInterval_ = time; }
		virtual void LocateParts();
		virtual void Update(shared_ptr<ScriptCommonDataManager>& commonDataManager);
	};

}
