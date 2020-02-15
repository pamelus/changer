#include <windows.h>
#include <stdio.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>

#include "kontrolki.h"

typedef struct
{
    HWND hwnd;
    bool isBreak;
    
    char pszPath[1024];
    
    char pszInput[1204];
    char pszOutput[1024];
} SELFSTRUCT;

DWORD WINAPI ScanThread(LPVOID param);
void SkanujKatalog(const char * pszPath, const char * pszInput, const char * pszOutput, bool & isBreak, HWND hwnd = NULL);
void SkanujPlik(const char * pszPath, const char * pszInput, const char * pszOutput);

BOOL CALLBACK MainDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool isWorking = false;
    switch(message)
    {
        case WM_INITDIALOG:
        {
            InitCommonControls();
            
            HICON hIcon = LoadIcon(GetModuleHandle(NULL), "A");
            SendMessage(hwnd, WM_SETICON, 0, (LPARAM)hIcon);
            DestroyIcon(hIcon);
            
            char pszBuffer[1024];
            FILE * stream = fopen("flower.conf", "r");
            if(stream)
            {
                if(fgets(pszBuffer, 1024, stream))
                {
                    for(unsigned long int i = 0; i < lstrlen(pszBuffer); i++)
                        if(pszBuffer[i] == '\n') pszBuffer[i] = 0;
                }
                else
                {
                    lstrcpy(pszBuffer, "");
                }
            
                fclose(stream);
            }
            else
            {
                   GetModuleFileName(GetModuleHandle(NULL), pszBuffer, 1024);
            }    
            SetDlgItemText(hwnd, ID_KATALOG, pszBuffer);
            
            EnableWindow(GetDlgItem(hwnd, IDC_STOP), false);
            EnableWindow(GetDlgItem(hwnd, ID_WYBOR), false);
            return FALSE;
        }
        case WM_CLOSE:
        {
            if(isWorking)
            {
                MessageBox(hwnd, "Program teraz pracuje i nie mo¿na zakoñczyæ jego dzia³ania.\nU¿yj przycisku [ Stop ] i spróbuj ponownie.", "ForFlower :)", MB_OK | MB_ICONWARNING);
            }
            else
            {
                char pszBuffer[1024];
                FILE * stream = fopen("flower.conf", "w");
                if(stream)
                {
                    GetDlgItemText(hwnd, ID_KATALOG, pszBuffer, 1024);
                    fputs(pszBuffer, stream);
                    fclose(stream);
                }
                EndDialog(hwnd, 0);
            }
            return TRUE;
        }
        case WM_COMMAND:
        {
            static SELFSTRUCT ss;
            switch(LOWORD(wParam))
            {
                case IDC_START:
                {                    
                    if(!GetDlgItemText(hwnd, ID_SOURCE, ss.pszInput, 1024))
                    {
                        MessageBox(hwnd, "Nie poda³aœ tekstu Ÿród³owego. Skanowanie anulowane.\nWpisz tekst Ÿród³owy i spróbuj ponownie.", "ForFlower :)", MB_OK | MB_ICONWARNING);
                        return TRUE;
                    }
                    
                    GetDlgItemText(hwnd, ID_OUTPUT, ss.pszOutput, 1024);
                    
                    GetDlgItemText(hwnd, ID_KATALOG, ss.pszPath, 1024);
                    if(!PathIsDirectory(ss.pszPath))
                    {
                        MessageBox(hwnd, "Podana œcie¿ka nie jest katalogiem. Skanowanie anulowane.\nWybierz jakiœ katalog i spróbuj ponownie.", "ForFlower :)", MB_OK | MB_ICONWARNING);
                        return TRUE;
                    }
                    
                    char pszText[1024];
                    wsprintf(pszText, "Wszystki pliki znajduj¹ce siê w katalogu [ %s ] i jego podkatalogach zostan¹ zmienione! Czy na pewno chcesz kontynuowaæ?", ss.pszPath);
                    if(MessageBox(hwnd, pszText, "ForFlower :)", MB_YESNO | MB_ICONQUESTION) == IDNO)
                        return TRUE;
                    
                    RECT rc; GetWindowRect(hwnd, &rc);
                    MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top + 20, true);
                    
                    EnableWindow(GetDlgItem(hwnd, ID_STATIC1), false);
                    EnableWindow(GetDlgItem(hwnd, ID_SOURCE), false);
                    EnableWindow(GetDlgItem(hwnd, ID_STATIC2), false);
                    EnableWindow(GetDlgItem(hwnd, ID_OUTPUT), false);
                    EnableWindow(GetDlgItem(hwnd, ID_STATIC3), false);
                    EnableWindow(GetDlgItem(hwnd, ID_KATALOG), false);
                    EnableWindow(GetDlgItem(hwnd, IDC_WYBIERZ), false);
                    EnableWindow(GetDlgItem(hwnd, ID_WYBOR), false);
                    EnableWindow(GetDlgItem(hwnd, IDC_START), false);
                    
                    HMENU hMenu = GetMenu(hwnd);                    
                    EnableMenuItem(hMenu, 0, MF_GRAYED | MF_BYPOSITION);
                    EnableMenuItem(hMenu, 1, MF_GRAYED | MF_BYPOSITION);
                    SetMenu(hwnd, hMenu);
                    
                    EnableWindow(GetDlgItem(hwnd, IDC_STOP), true);
                    isWorking = true;
                    
                    GetClientRect(hwnd, &rc);
                    HWND hProgres = CreateWindowEx(0, PROGRESS_CLASS, "", WS_CHILD | WS_VISIBLE, 5, rc.bottom - rc.top - 20, rc.right - rc.left - 10, 18, hwnd, (HMENU)9999, GetModuleHandle(NULL), NULL);
                    SendMessage(hProgres, PBM_SETRANGE32, 0, 100000);
                    SendMessage(hProgres, PBM_SETSTEP, (WPARAM)1, 0);
                    
                    ss.hwnd = hwnd;
                    ss.isBreak = false;
                    
                    
                    
                    CreateThread(NULL, 0, ScanThread, (LPVOID)&ss, 0, NULL);
                    return TRUE;
                }
                case IDC_STOP:
                {
                    ss.isBreak = true;
                    return TRUE;
                }
                case IDC_CLEAN:
                {
                    DestroyWindow(GetDlgItem(hwnd, 9999));
                    RECT rc; GetWindowRect(hwnd, &rc);
                    MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top - 20, true);
                    
                    EnableWindow(GetDlgItem(hwnd, ID_STATIC1), true);
                    EnableWindow(GetDlgItem(hwnd, ID_SOURCE), true);
                    EnableWindow(GetDlgItem(hwnd, ID_STATIC2), true);
                    EnableWindow(GetDlgItem(hwnd, ID_OUTPUT), true);
                    EnableWindow(GetDlgItem(hwnd, ID_STATIC3), true);
                    EnableWindow(GetDlgItem(hwnd, ID_KATALOG), true);
                    EnableWindow(GetDlgItem(hwnd, IDC_WYBIERZ), true);
                    EnableWindow(GetDlgItem(hwnd, IDC_START), true);
                    
                    HMENU hMenu = GetMenu(hwnd);                    
                    EnableMenuItem(hMenu, 0, MF_ENABLED | MF_BYPOSITION);
                    EnableMenuItem(hMenu, 1, MF_ENABLED | MF_BYPOSITION);
                    SetMenu(hwnd, hMenu);
                    
                    EnableWindow(GetDlgItem(hwnd, IDC_STOP), false);
                    
                    isWorking = false;
                    return TRUE;
                }
                case IDC_WYBIERZ:
                {
                    char pszPath[1024]; GetDlgItemText(hwnd, ID_KATALOG, pszPath, 1024);
                    
                    LPITEMIDLIST pidl;
                    
                    BROWSEINFO bi; ZeroMemory(&bi, sizeof(BROWSEINFO));
                    bi.hwndOwner = hwnd;
                    bi.pidlRoot = NULL;
                    bi.pszDisplayName = pszPath;
                    bi.lpszTitle = "Wybierz folder:";
                    bi.ulFlags = BIF_RETURNONLYFSDIRS;
                    
                    if(pidl = SHBrowseForFolder(&bi))
                    {
                        SHGetPathFromIDList(pidl, pszPath);
                        SetDlgItemText(hwnd, ID_KATALOG, pszPath);
                    }
                    
                    return TRUE;
                }
            }
            return FALSE;
        }
    }
    return FALSE;
}

int WinMain(HINSTANCE hThis, HINSTANCE hPrev, char * pszCmd, int nShow)
{
    return DialogBox(hThis, "MAIN_DIALOG", HWND_DESKTOP, MainDialogProc);
}

DWORD WINAPI ScanThread(LPVOID param)
{
    SELFSTRUCT * pss = (SELFSTRUCT *)param;
    
    SetCurrentDirectory(pss->pszPath);
    SetCurrentDirectory("..");
    
    lstrcpy(pss->pszPath, PathFindFileName(pss->pszPath));
    
    for(unsigned long int i = 0; i < lstrlen(pss->pszInput); i++)
    {
        if(pss->pszInput[i] == '\n' || pss->pszInput[i] == '\r' || pss->pszInput[i] == '\t' || pss->pszInput[i] == '\a' || pss->pszInput[i] == '\b' || pss->pszInput[i] == '\f' || pss->pszInput[i] == '\v')
            pss->pszInput[i] == '^';
    }
    
    SkanujKatalog(pss->pszPath, pss->pszInput, pss->pszOutput, pss->isBreak, pss->hwnd);
    SendMessage(pss->hwnd, WM_COMMAND, IDC_CLEAN, 0);
    return 0;
}

void SkanujKatalog(const char * pszPath, const char * pszInput, const char * pszOutput, bool & isBreak, HWND hwnd)
{
    if(isBreak) return;
    
    if(PathIsDirectory(pszPath))
    {
        if(hwnd)
        {
            SetCurrentDirectory(pszPath);
            
            unsigned long int range = 0;
            WIN32_FIND_DATA wfd; ZeroMemory(&wfd, sizeof(WIN32_FIND_DATA));
            HANDLE hFile = FindFirstFile("*.*", &wfd);
            do
            {
                if(wfd.cFileName[0] == '.') continue;
                    range++;
            }
            while(FindNextFile(hFile, &wfd));
            FindClose(hFile);
            
            SetCurrentDirectory("..");
            
            SendMessage(GetDlgItem(hwnd, 9999), PBM_SETRANGE32, 0, range);
        }
        
        SetCurrentDirectory(pszPath);
        
        WIN32_FIND_DATA wfd; ZeroMemory(&wfd, sizeof(WIN32_FIND_DATA));
        HANDLE hFile = FindFirstFile("*.*", &wfd);
        do
        {
            if(wfd.cFileName[0] == '.') continue;
            SkanujKatalog(wfd.cFileName, pszInput, pszOutput, isBreak);
            if(isBreak) break; if(hwnd) SendMessage(GetDlgItem(hwnd, 9999), PBM_STEPIT, 0, 0);
        }
        while(FindNextFile(hFile, &wfd));
        FindClose(hFile);
        
        SetCurrentDirectory("..");
    }
    else
    {
        SkanujPlik(pszPath, pszInput, pszOutput);
    }
}

void SkanujPlik(const char * pszPath, const char * pszInput, const char * pszOutput)
{
    FILE * stream = fopen(pszPath, "r");
    if(stream)
    {
        char pszBuffer[102400]; unsigned long int i = 0;
        while(!feof(stream))
        {
            pszBuffer[i] = fgetc(stream);
            i++;
            if(i >= 102399)
            {
                fclose(stream);
                return;
            }
        }
        pszBuffer[i] = 0;
        fclose(stream);
        
        char pszBufferReal[102400];
        lstrcpy(pszBufferReal, pszBuffer);
        
        for(unsigned long int i = 0; i < lstrlen(pszBuffer); i++)
        {
            if(pszBuffer[i] == '\n' || pszBuffer[i] == '\r' || pszBuffer[i] == '\t' || pszBuffer[i] == '\a' || pszBuffer[i] == '\b' || pszBuffer[i] == '\f' || pszBuffer[i] == '\v')
                pszBuffer[i] == '^';
        }
        
        unsigned long int len = strlen(pszInput);
        for(unsigned long int i = 0; i < strlen(pszBuffer) - len; i++)
        {
            if(strncmp(&pszBuffer[i], pszInput, len) == 0)
            {
                char pszPart1[i+1];
                lstrcpyn(pszPart1, pszBufferReal, i+1);
                
                char pszPart2[lstrlen(pszBufferReal) - (i + len) + 1];
                lstrcpy(pszPart2, &pszBufferReal[i+len]);
                
                lstrcpy(pszBufferReal, pszPart1);
                lstrcat(pszBufferReal, pszOutput);
                lstrcat(pszBufferReal, pszPart2);
                
                lstrcpyn(pszPart1, pszBuffer, i+1);
                lstrcpy(pszPart2, &pszBuffer[i+len]);
                
                lstrcpy(pszBuffer, pszPart1);
                lstrcat(pszBuffer, pszOutput);
                lstrcat(pszBuffer, pszPart2);
                i += strlen(pszOutput) - 1;
            }
        }
        
        pszBufferReal[lstrlen(pszBufferReal) - 1] = 0;
        
        stream = fopen(pszPath, "w");
        fputs(pszBufferReal, stream);
        fclose(stream);
    }
}
