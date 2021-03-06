
#include "stdafx.h"
#include "../CaCommon/WixString.h"
#include <sqlutil.h>
#include <atlbase.h>
#include <memory>
using namespace std;

#define SqlSearchQuery L"SELECT `Property_`, `Server`, `Instance`, `Database`, `Username`, `Password`, `Query` FROM `PSW_SqlSearch`"
enum eSqlSearchQueryQuery { Property_ = 1, Server, Instance, Database, Username, Password, Query };

struct DBCOLUMNDATA
{
	DBLENGTH dwLength;
	DBSTATUS dwStatus;
	BYTE bData[1];
};

extern "C" UINT __stdcall SqlSearch(MSIHANDLE hInstall)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;
	PMSIHANDLE hView;
	PMSIHANDLE hRecord;
	hr = WcaInitialize(hInstall, __FUNCTION__);
	BreakExitOnFailure(hr, "Failed to initialize");
	WcaLog(LOGMSG_STANDARD, "Initialized from PanelSwCustomActions " FullVersion);

	hr = WcaTableExists(L"PSW_SqlSearch");
	BreakExitOnFailure(hr, "Table does not exist 'PSW_SqlSearch'. Have you authored 'PanelSw:SqlSearch' entries in WiX code?");

	// Execute view
	hr = WcaOpenExecuteView(SqlSearchQuery, &hView);
	BreakExitOnFailure(hr, "Failed to execute SQL query on 'PSW_SqlSearch'.");

	// Iterate records
	while ((hr = WcaFetchRecord(hView, &hRecord)) != E_NOMOREITEMS)
	{
		BreakExitOnFailure(hr, "Failed to fetch record.");

		// Get fields
		CWixString szProperty;
		CWixString szServer;
		CWixString szInstance;
		CWixString szDatabase;
		CWixString szUsername;
		CWixString szPassword;
		CWixString szQuery;
		DBCOUNTITEM unRows = 0;
		CComBSTR szError;
		CComPtr<IDBCreateSession> pDbSession;
		CComPtr<IRowset> pRowset;
		CComPtr<IAccessor> pAccessor;
		CComPtr<IColumnsInfo> pColumnsInfo;
		DBORDINAL nColCount = 0;
		CComBSTR szColNames;
		DBBINDING sDbBinding;
		DBCOLUMNDATA *pColumn = nullptr;
		HACCESSOR hAccessor = NULL;
		DBBINDSTATUS nBindStatus = DBBINDSTATUS_OK;
		unique_ptr<BYTE, void(__stdcall*)(LPVOID)> pRowData(nullptr, ::CoTaskMemFree);
		unique_ptr<DBCOLUMNINFO, void(__stdcall*)(LPVOID)> pColInfo(nullptr, ::CoTaskMemFree);
		unique_ptr<HROW, void(__stdcall*)(LPVOID)> phRow(nullptr, ::CoTaskMemFree);

		hr = WcaGetRecordString(hRecord, eSqlSearchQueryQuery::Property_, (LPWSTR*)szProperty);
		BreakExitOnFailure(hr, "Failed to get Property_.");
		hr = WcaGetRecordFormattedString(hRecord, eSqlSearchQueryQuery::Server, (LPWSTR*)szServer);
		BreakExitOnFailure(hr, "Failed to get Server.");
		hr = WcaGetRecordFormattedString(hRecord, eSqlSearchQueryQuery::Instance, (LPWSTR*)szInstance);
		BreakExitOnFailure(hr, "Failed to get Instance.");
		hr = WcaGetRecordFormattedString(hRecord, eSqlSearchQueryQuery::Database, (LPWSTR*)szDatabase);
		BreakExitOnFailure(hr, "Failed to get Database.");
		hr = WcaGetRecordFormattedString(hRecord, eSqlSearchQueryQuery::Username, (LPWSTR*)szUsername);
		BreakExitOnFailure(hr, "Failed to get Username.");
		hr = WcaGetRecordFormattedString(hRecord, eSqlSearchQueryQuery::Password, (LPWSTR*)szPassword);
		BreakExitOnFailure(hr, "Failed to get Password.");
		hr = WcaGetRecordFormattedString(hRecord, eSqlSearchQueryQuery::Query, (LPWSTR*)szQuery);
		BreakExitOnFailure(hr, "Failed to get Query.");

		// Handle:
		// Server\Instance
		// Server\...\Instance (multiple backslash)
		// Server\ (default instance)
		if (szInstance.IsNullOrEmpty())
		{
			DWORD dwInstIndex = 0;

			dwInstIndex = szServer.RFind(L'\\');
			if ((dwInstIndex != INFINITE) && (dwInstIndex < szServer.StrLen()))
			{
				// Copy instance name
				hr = szInstance.Copy(dwInstIndex + 1 + (LPCWSTR)szServer);
				BreakExitOnFailure(hr, "Failed copying instance name.");

				// Terminate server name on first backslash
				dwInstIndex = szServer.Find(L'\\');
				const_cast<LPWSTR>((LPCWSTR)szServer)[dwInstIndex] = NULL;
			}
		}

		WcaLog(LOGLEVEL::LOGMSG_STANDARD, "Executing SQL query '%ls'. Server='%ls', Instance='%ls', User='%ls'. Will place results in property '%ls'", (LPCWSTR)szQuery, (LPCWSTR)szServer, (LPCWSTR)szInstance, (LPCWSTR)szUsername, (LPCWSTR)szProperty);

		hr = SqlConnectDatabase((LPCWSTR)szServer, (LPCWSTR)szInstance, (LPCWSTR)szDatabase, szUsername.IsNullOrEmpty(), (LPCWSTR)szUsername, (LPCWSTR)szPassword, &pDbSession);
		BreakExitOnFailure(hr, "Failed connecting to database");
		BreakExitOnNull(pDbSession, hr, E_FAIL, "Failed connecting to database (NULL)");

		hr = SqlSessionExecuteQuery(pDbSession, (LPCWSTR)szQuery, &pRowset, nullptr, &szError);
		BreakExitOnFailure(hr, "Failed executing query. %ls", (LPCWSTR)(LPWSTR)szError);
		BreakExitOnNull(pRowset, hr, E_FAIL, "Failed executing query (NULL)");

		// Requesting 2 rows to ensure one is returned at most.
		hr = pRowset->GetNextRows(DB_NULL_HCHAPTER, 0, 2, &unRows, &phRow._Myptr());
		BreakExitOnFailure(hr, "Failed to get result row");
		BreakExitOnNull((unRows <= 1), hr, E_FAIL, "Query returned more than one row");

		// Empty result ==> clear property
		if (unRows == 0)
		{
			WcaLog(LOGLEVEL::LOGMSG_STANDARD, "Query returned no results. Clearing property");

			hr = WcaSetProperty((LPCWSTR)szProperty, L"");
			BreakExitOnFailure(hr, "Failed clearing property '%ls'", (LPCWSTR)szProperty);
			continue;
		}

		// Get column info
		hr = pRowset->QueryInterface<IColumnsInfo>(&pColumnsInfo);
		BreakExitOnFailure(hr, "Failed to get result column info");

		hr = pColumnsInfo->GetColumnInfo(&nColCount, &pColInfo._Myptr(), &szColNames);
		BreakExitOnFailure(hr, "Failed to get result column info");
		BreakExitOnNull((nColCount <= 1), hr, E_INVALIDARG, "Query returned %i columns. Can only handle scalar queries", nColCount);

		if (nColCount == 0)
		{
			WcaLog(LOGLEVEL::LOGMSG_STANDARD, "Query returned no results. Clearing property");

			hr = WcaSetProperty((LPCWSTR)szProperty, L"");
			BreakExitOnFailure(hr, "Failed clearing property '%ls'", (LPCWSTR)szProperty);
			continue;
		}

		sDbBinding.dwPart = DBPART_VALUE | DBPART_LENGTH | DBPART_STATUS;
		sDbBinding.eParamIO = DBPARAMIO_NOTPARAM;
		sDbBinding.iOrdinal = pColInfo->iOrdinal;
		sDbBinding.wType = DBTYPE_WSTR;
		sDbBinding.pTypeInfo = nullptr;
		sDbBinding.obValue = offsetof(DBCOLUMNDATA, bData);
		sDbBinding.obLength = offsetof(DBCOLUMNDATA, dwLength);
		sDbBinding.obStatus = offsetof(DBCOLUMNDATA, dwStatus);
		sDbBinding.cbMaxLen = pColInfo->ulColumnSize;
		sDbBinding.pObject = nullptr;
		sDbBinding.pBindExt = nullptr;
		sDbBinding.dwFlags = 0;
		sDbBinding.dwMemOwner = DBMEMOWNER_CLIENTOWNED;
		sDbBinding.bPrecision = 0;
		sDbBinding.bScale = 0;

		// Create accessor
		hr = pRowset->QueryInterface<IAccessor>(&pAccessor);
		BreakExitOnFailure(hr, "Failed to get result row accessor");

		hr = pAccessor->CreateAccessor(DBACCESSOR_ROWDATA, 1, &sDbBinding, sizeof(DBCOLUMNDATA), &hAccessor, &nBindStatus);
		BreakExitOnFailure(hr, "Failed to create column data accessor (DBBINDSTATUS=%i)", nBindStatus);

		pRowData.reset((BYTE*)::CoTaskMemAlloc(sDbBinding.cbMaxLen + sizeof(DBCOLUMNDATA)));
		BreakExitOnNull(pRowData, hr, E_FAIL, "Failed to allocate memory");

		hr = pRowset->GetData(phRow.get()[0], hAccessor, pRowData.get());
		BreakExitOnFailure(hr, "Failed to get row data");

		hr = pAccessor->ReleaseAccessor(hAccessor, nullptr);
		BreakExitOnFailure(hr, "Failed releasing accessor");
		hAccessor = NULL;

		pColumn = (DBCOLUMNDATA*)(pRowData.get() + sDbBinding.obLength);
		switch (pColumn->dwStatus)
		{
		case DBSTATUS_S_ISNULL:
			WcaLog(LOGLEVEL::LOGMSG_STANDARD, "Query result is null. Clearing property");
			hr = WcaSetProperty((LPCWSTR)szProperty, L"");
			BreakExitOnFailure(hr, "Failed clearing property '%ls'", (LPCWSTR)szProperty);
			break;

		case DBSTATUS_S_OK:
			WcaLog(LOGLEVEL::LOGMSG_STANDARD, "Query returned '%ls'", pColumn->bData ? (LPCWSTR)pColumn->bData : L"<null>");
			hr = WcaSetProperty((LPCWSTR)szProperty, pColumn->bData ? (LPCWSTR)pColumn->bData : L"");
			BreakExitOnFailure(hr, "Failed setting property '%ls'", (LPCWSTR)szProperty);
			break;

		case DBSTATUS_S_TRUNCATED:
			WcaLog(LOGLEVEL::LOGMSG_STANDARD, "Query returned truncated data '%ls'", pColumn->bData ? (LPCWSTR)pColumn->bData : L"<null>");
			hr = WcaSetProperty((LPCWSTR)szProperty, pColumn->bData ? (LPCWSTR)pColumn->bData : L"");
			BreakExitOnFailure(hr, "Failed setting property '%ls'", (LPCWSTR)szProperty);
			break;

		case DBSTATUS_E_BADACCESSOR:
		case DBSTATUS_E_CANTCONVERTVALUE:
		case DBSTATUS_E_SIGNMISMATCH:
		case DBSTATUS_E_DATAOVERFLOW:
		case DBSTATUS_E_CANTCREATE:
		case DBSTATUS_E_UNAVAILABLE:
		case DBSTATUS_E_PERMISSIONDENIED:
		case DBSTATUS_E_INTEGRITYVIOLATION:
		case DBSTATUS_E_SCHEMAVIOLATION:
		case DBSTATUS_E_BADSTATUS:
		case DBSTATUS_S_DEFAULT: // Unexpected when getting data
		default:
			hr = min(-1, -::abs((long)pColumn->dwStatus));
			BreakExitOnFailure(hr, "Failed getting column data");
			break;
		}
	}
	hr = S_OK;


LExit:

	er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
	return WcaFinalize(er);
}