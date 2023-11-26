#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <algorithm>

#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ext/stb/stb_image_write.h"
#define SDF_IMPLEMENTATION
#include "sdf/sdf.h"

inline bool ends_with(std::string const &value, std::string const &ending)
{
	if (ending.size() > value.size())
		return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

//--------------------------------------------------------------------------------------
// Entry point to the program
//--------------------------------------------------------------------------------------
int __cdecl wmain()
{
	OPENFILENAME ofn;       // common dialog box structure
	char szFile[MAX_PATH] = { '\0' };       // buffer for file name

	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "ALL\0*.*\0PNG\0*.png\0TGA\0*.tga\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	// Display the Open dialog box. 
	if (GetOpenFileName(&ofn))
	{
		printf("Selected file: %s\n", szFile);
	}
	else
	{
		return 0;
	}

	// Data Prepare
	unsigned int SizeX, SizeY, Comp;
	int sizeX, sizeY, comp;
	unsigned char *charData = stbi_load(szFile, &sizeX, &sizeY, &comp, 4);
	if (comp != 4)
	{
		return 0;
	}
	SizeX = sizeX;
	SizeY = sizeY;
	Comp = comp;
	unsigned int elementSize = SizeX * SizeY * Comp;
	unsigned int pixelSize = SizeX * SizeY;

	unsigned char *alphaCharData = new unsigned char[pixelSize];
	for (unsigned int i = 0; i < pixelSize; i++)
	{
		alphaCharData[i] = charData[i * 4 + 3];
	}

	unsigned char *newAlphaCharData = new unsigned char[pixelSize];
	sdfBuildDistanceField(newAlphaCharData, SizeX, SizeX / 4, SizeX / 4, alphaCharData, SizeX, SizeY, SizeX);

	unsigned char *newCharData = new unsigned char[elementSize];
	for (unsigned int i = 0; i < pixelSize; i++)
	{
		newCharData[i * 4] = charData[i * 4];
		newCharData[i * 4 + 1] = charData[i * 4 + 1];
		newCharData[i * 4 + 2] = charData[i * 4 + 2];
		newCharData[i * 4 + 3] = newAlphaCharData[i];
	}

	std::string filename(szFile);
	std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
	if (ends_with(filename, ".png"))
	{
		stbi_write_png(szFile, SizeX, SizeY, comp, newCharData, SizeX * 4);
	}
	else if (ends_with(filename, ".tga"))
	{
		stbi_write_tga(szFile, SizeX, SizeY, comp, newCharData);
	}

	stbi_image_free(charData);
	delete[] alphaCharData;
	delete[] newCharData;
	delete[] newAlphaCharData;

	printf("Write sdf data to selected file success...\n");

	return 0;
}
