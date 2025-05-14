#include "framework.h"
#include "WindowsProject3.h"
#include <winhttp.h>
#include <string>
#include <vector>
#include <commctrl.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <locale>
#include <codecvt>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comctl32.lib")

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

#define IDC_LISTVIEW           1001
#define IDC_BUTTON_REFRESH     1002
#define IDC_BUTTON_SORT_HIGH   1003
#define IDC_BUTTON_SORT_LOW    1004
#define IDC_COMBO_CURRENCY     1005

std::wstring currentCurrency = L"USD";

struct CryptoCurrency {
    std::wstring name;
    std::wstring price;
    std::wstring change24h;
    double priceValue;
};

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void AddColumnsToListView(HWND hList);
void FillListViewWithData(HWND hList, const std::vector<CryptoCurrency>& data);
std::wstring DownloadCryptoData(const std::wstring& currency);
std::vector<CryptoCurrency> ParseCryptoData(const std::wstring& json, const std::wstring& currency);
void SortDataByPrice(std::vector<CryptoCurrency>& data, bool sortDescending);
LPWSTR MakeLPWSTR(const wchar_t* str);

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_WINDOWSPROJECT3, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
        return FALSE;

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex = {
        sizeof(WNDCLASSEX),
        CS_HREDRAW | CS_VREDRAW,
        WndProc,
        0, 0,
        hInstance,
        LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION)),
        LoadCursor(nullptr, IDC_ARROW),
        (HBRUSH)(COLOR_WINDOW + 1),
        nullptr,
        szWindowClass,
        LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION))
    };
    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;
    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;

    HWND hList = CreateWindowW(WC_LISTVIEW, L"", WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT,
        10, 50, 760, 450, hWnd, (HMENU)IDC_LISTVIEW, hInstance, NULL);

    AddColumnsToListView(hList);

    CreateWindowW(L"BUTTON", L"Обновить", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10, 10, 100, 30, hWnd, (HMENU)IDC_BUTTON_REFRESH, hInstance, NULL);

    CreateWindowW(L"BUTTON", L"По возрастанию", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        120, 10, 150, 30, hWnd, (HMENU)IDC_BUTTON_SORT_LOW, hInstance, NULL);

    CreateWindowW(L"BUTTON", L"По убыванию", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        280, 10, 150, 30, hWnd, (HMENU)IDC_BUTTON_SORT_HIGH, hInstance, NULL);

    HWND hCombo = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
        440, 10, 100, 200, hWnd, (HMENU)IDC_COMBO_CURRENCY, hInstance, NULL);
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"USD");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"EUR");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"RUB");
    SendMessageW(hCombo, CB_SETCURSEL, 0, 0);

    std::wstring json = DownloadCryptoData(currentCurrency);
    auto data = ParseCryptoData(json, currentCurrency);
    FillListViewWithData(hList, data);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

std::wstring DownloadCryptoData(const std::wstring& currency) {
    HINTERNET hSession = WinHttpOpen(L"CryptoApp/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return L"";

    std::wstring url = L"/api/v3/coins/markets?vs_currency=" + currency +
        L"&order=market_cap_desc&sparkline=false&price_change_percentage=24h";

    HINTERNET hConnect = WinHttpConnect(hSession, L"api.coingecko.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) return L"";

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", url.c_str(), NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) return L"";

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
        0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResults || !WinHttpReceiveResponse(hRequest, NULL)) return L"";

    DWORD dwSize = 0;
    std::string result;
    do {
        DWORD dwDownloaded = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (!dwSize) break;

        char* buffer = new char[dwSize + 1];
        ZeroMemory(buffer, dwSize + 1);
        WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded);
        result.append(buffer, dwDownloaded);
        delete[] buffer;
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return std::wstring(result.begin(), result.end());
}

std::vector<CryptoCurrency> ParseCryptoData(const std::wstring& json, const std::wstring& currency) {
    std::vector<CryptoCurrency> coins;
    size_t pos = 0;
    while ((pos = json.find(L"\"id\":", pos)) != std::wstring::npos) {
        CryptoCurrency coin;

        size_t id_start = json.find(L"\"", pos + 5) + 1;
        size_t id_end = json.find(L"\"", id_start);
        std::wstring coin_id = json.substr(id_start, id_end - id_start);

        if (coin_id == L"bitcoin") coin.name = L"Bitcoin (BTC)";
        else if (coin_id == L"ethereum") coin.name = L"Ethereum (ETH)";
        else if (coin_id == L"litecoin") coin.name = L"Litecoin (LTC)";
        else coin.name = coin_id;

        size_t price_pos = json.find(L"\"current_price\":", id_end);
        if (price_pos != std::wstring::npos) {
            size_t price_start = json.find_first_not_of(L" :", price_pos + 16);
            size_t price_end = json.find_first_of(L",}", price_start);
            std::wstring price_str = json.substr(price_start, price_end - price_start);

            try {
                coin.priceValue = std::stod(price_str);
                std::wstringstream ss;
                ss << std::fixed << std::setprecision(2) << coin.priceValue;
                coin.price = ss.str() + L" " + currency;
            }
            catch (...) {
                coin.price = L"N/A";
                coin.priceValue = 0;
            }
        }

        size_t change_pos = json.find(L"\"price_change_percentage_24h\":", price_pos);
        if (change_pos != std::wstring::npos) {
            size_t change_start = json.find_first_not_of(L" :", change_pos + 29);
            size_t change_end = json.find_first_of(L",}", change_start);
            std::wstring change_str = json.substr(change_start, change_end - change_start);
            try {
                double change = std::stod(change_str);
                std::wstringstream ss;
                if (change > 0) ss << L"+";
                ss << std::fixed << std::setprecision(2) << change << L"%";
                coin.change24h = ss.str();
            }
            catch (...) {
                coin.change24h = L"N/A";
            }
        }

        coins.push_back(coin);
        pos = change_pos;
    }
    return coins;
}

void FillListViewWithData(HWND hList, const std::vector<CryptoCurrency>& data) {
    ListView_DeleteAllItems(hList);
    for (size_t i = 0; i < data.size(); ++i) {
        LVITEM lvItem = { 0 };
        lvItem.mask = LVIF_TEXT;
        lvItem.iItem = i;
        lvItem.iSubItem = 0;
        lvItem.pszText = (LPWSTR)data[i].name.c_str();
        ListView_InsertItem(hList, &lvItem);
        ListView_SetItemText(hList, i, 1, (LPWSTR)data[i].price.c_str());
        ListView_SetItemText(hList, i, 2, (LPWSTR)data[i].change24h.c_str());
    }
}

void AddColumnsToListView(HWND hList) {
    LVCOLUMN lvc = { 0 };
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;

    LPWSTR col1 = MakeLPWSTR(L"Криптовалюта");
    lvc.iSubItem = 0;
    lvc.pszText = col1;
    lvc.cx = 200;
    ListView_InsertColumn(hList, 0, &lvc);
    delete[] col1;

    LPWSTR col2 = MakeLPWSTR(L"Цена");
    lvc.iSubItem = 1;
    lvc.pszText = col2;
    lvc.cx = 150;
    ListView_InsertColumn(hList, 1, &lvc);
    delete[] col2;

    LPWSTR col3 = MakeLPWSTR(L"Изменение (24ч)");
    lvc.iSubItem = 2;
    lvc.pszText = col3;
    lvc.cx = 150;
    ListView_InsertColumn(hList, 2, &lvc);
    delete[] col3;
}

void SortDataByPrice(std::vector<CryptoCurrency>& data, bool sortDescending) {
    std::sort(data.begin(), data.end(), [sortDescending](const CryptoCurrency& a, const CryptoCurrency& b) {
        return sortDescending ? a.priceValue > b.priceValue : a.priceValue < b.priceValue;
        });
}

LPWSTR MakeLPWSTR(const wchar_t* str) {
    size_t len = wcslen(str) + 1;
    LPWSTR lp = new WCHAR[len];
    wcscpy_s(lp, len, str);
    return lp;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static std::vector<CryptoCurrency> currentData;

    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BUTTON_REFRESH:
        {
            HWND hList = GetDlgItem(hWnd, IDC_LISTVIEW);
            std::wstring json = DownloadCryptoData(currentCurrency);
            currentData = ParseCryptoData(json, currentCurrency);
            FillListViewWithData(hList, currentData);
            break;
        }

        case IDC_BUTTON_SORT_LOW:
        case IDC_BUTTON_SORT_HIGH:
        {
            HWND hList = GetDlgItem(hWnd, IDC_LISTVIEW);
            bool descending = LOWORD(wParam) == IDC_BUTTON_SORT_HIGH;
            SortDataByPrice(currentData, descending);
            FillListViewWithData(hList, currentData);
            break;
        }

        default:
            break;
        }

        if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_COMBO_CURRENCY)
        {
            HWND hCombo = (HWND)lParam;
            wchar_t currency[10];
            int index = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
            SendMessageW(hCombo, CB_GETLBTEXT, index, (LPARAM)currency);
            currentCurrency = currency;

            std::wstring json = DownloadCryptoData(currentCurrency);
            currentData = ParseCryptoData(json, currentCurrency);
            HWND hList = GetDlgItem(hWnd, IDC_LISTVIEW);
            FillListViewWithData(hList, currentData);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
