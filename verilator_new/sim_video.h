#pragma once

#include <string>
#ifndef _MSC_VER
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl2.h"
#else
#define WIN32
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#endif

struct SimVideo {
public:

	int output_width;
	int output_height;
	int output_rotate;

	int xMax;
	int xMin;
	int yMax;
	int yMin;

	ImTextureID texture_id;
	ImGuiIO io;

	SimVideo(int width, int height);
	~SimVideo();
	void UpdateTexture();
	void CleanUp();
	void StartFrame();
	void Output(int x, int y, uint32_t colour);
	int Initialise(const char* windowTitle);
};
