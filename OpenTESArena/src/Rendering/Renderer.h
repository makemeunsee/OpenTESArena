#ifndef RENDERER_H
#define RENDERER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "RendererSystem2D.h"
#include "RendererSystem3D.h"
#include "RendererSystemType.h"
#include "../Assets/ArenaTypes.h"
#include "../Entities/EntityManager.h"
#include "../Math/Vector2.h"
#include "../Math/Vector3.h"
#include "../Media/TextureUtils.h"
#include "../UI/Texture.h"

// Container for 2D and 3D rendering operations.

class Color;
class EntityAnimationDefinition;
class EntityAnimationInstance;
class EntityDefinitionLibrary;
class EntityManager;
class Rect;
class Surface;
class TextureManager;
class WeatherInstance;

enum class CursorAlignment;

struct SDL_Rect;
struct SDL_Renderer;
struct SDL_Surface;
struct SDL_Texture;
struct SDL_Window;

class Renderer
{
public:
	struct DisplayMode
	{
		int width, height, refreshRate;

		DisplayMode(int width, int height, int refreshRate);
	};

	enum class WindowMode
	{
		Window,
		BorderlessFullscreen,
		ExclusiveFullscreen
	};

	// Profiler information from the most recently rendered frame.
	struct ProfilerData
	{
		// Internal renderer resolution.
		int width, height;

		int threadCount;

		// Visible flats and lights.
		int potentiallyVisFlatCount, visFlatCount, visLightCount;

		double frameTime;

		ProfilerData();

		void init(int width, int height, int threadCount, int potentiallyVisFlatCount,
			int visFlatCount, int visLightCount, double frameTime);
	};

	using ResolutionScaleFunc = std::function<double()>;
private:
	static const char *DEFAULT_RENDER_SCALE_QUALITY;
	static const char *DEFAULT_TITLE;

	std::unique_ptr<RendererSystem2D> renderer2D;
	std::unique_ptr<RendererSystem3D> renderer3D;
	std::vector<DisplayMode> displayModes;
	SDL_Window *window;
	SDL_Renderer *renderer;
	Texture nativeTexture, gameWorldTexture; // Frame buffers.
	ProfilerData profilerData;
	ResolutionScaleFunc resolutionScaleFunc; // Gets an up-to-date resolution scale value from the game options.
	int letterboxMode; // Determines aspect ratio of the original UI (16:10, 4:3, etc.).
	bool fullGameWindow; // Determines height of 3D frame buffer.

	// Helper method for making a renderer context.
	static SDL_Renderer *createRenderer(SDL_Window *window);

	// Generates a renderer dimension while avoiding pitfalls of numeric imprecision.
	static int makeRendererDimension(int value, double resolutionScale);
public:
	// Only defined so members are initialized for Game ctor exception handling.
	Renderer();
	~Renderer();

	// Default bits per pixel.
	static const int DEFAULT_BPP;

	// The default pixel format for all software surfaces, ARGB8888.
	static const uint32_t DEFAULT_PIXELFORMAT;

	// Gets the letterbox aspect associated with the current letterbox mode.
	double getLetterboxAspect() const;

	// Gets the width and height of the active window.
	Int2 getWindowDimensions() const;

	// Gets the aspect ratio of the active window.
	double getWindowAspect() const;

	// Gets a list of supported fullscreen display modes.
	const std::vector<DisplayMode> &getDisplayModes() const;

	// Gets the active window's pixels-per-inch scale divided by platform DPI.
	double getDpiScale() const;

	// The "view height" is the height in pixels for the visible game world. This 
	// depends on whether the whole screen is rendered or just the portion above 
	// the interface. The game interface is 53 pixels tall in 320x200.
	Int2 getViewDimensions() const;

	// This is for the "letterbox" part of the screen, scaled to fit the window 
	// using the given letterbox aspect.
	SDL_Rect getLetterboxDimensions() const;

	// Gets a screenshot of the current window.
	Surface getScreenshot() const;

	// Gets profiler data (timings, renderer properties, etc.).
	const ProfilerData &getProfilerData() const;

	// Tests whether an entity is intersected by the given ray. Intended for ray cast selection.
	// 'pixelPerfect' determines whether the entity's texture is involved in the calculation.
	// Returns whether the entity was able to be tested and was hit by the ray. This is a renderer
	// function because the exact method of testing may depend on the 3D representation of the entity.
	bool getEntityRayIntersection(const EntityVisibilityState3D &visState, const EntityDefinition &entityDef,
		const VoxelDouble3 &entityForward, const VoxelDouble3 &entityRight, const VoxelDouble3 &entityUp,
		double entityWidth, double entityHeight, const CoordDouble3 &rayPoint, const VoxelDouble3 &rayDirection,
		bool pixelPerfect, const Palette &palette, CoordDouble3 *outHitPoint) const;

	// Converts a [0, 1] screen point to a ray through the world. The exact direction is
	// dependent on renderer details.
	Double3 screenPointToRay(double xPercent, double yPercent, const Double3 &cameraDirection,
		double fovY, double aspect) const;

	// Transforms a native window (i.e., 1920x1080) point or rectangle to an original 
	// (320x200) point or rectangle. Points outside the letterbox will either be negative 
	// or outside the 320x200 limit when returned.
	Int2 nativeToOriginal(const Int2 &nativePoint) const;
	Rect nativeToOriginal(const Rect &nativeRect) const;

	// Does the opposite of nativeToOriginal().
	Int2 originalToNative(const Int2 &originalPoint) const;
	Rect originalToNative(const Rect &originalRect) const;

	// Returns true if the letterbox contains a native point.
	bool letterboxContains(const Int2 &nativePoint) const;

	// Wrapper methods for SDL_CreateTexture.
	Texture createTexture(uint32_t format, int access, int w, int h);
	Texture createTextureFromSurface(const Surface &surface);

	bool init(int width, int height, WindowMode windowMode, int letterboxMode, const ResolutionScaleFunc &resolutionScaleFunc,
		RendererSystemType2D systemType2D, RendererSystemType3D systemType3D);

	// Resizes the renderer dimensions.
	void resize(int width, int height, double resolutionScale, bool fullGameWindow);

	// Sets the letterbox mode.
	void setLetterboxMode(int letterboxMode);

	// Sets whether the program is windowed, fullscreen, etc..
	void setWindowMode(WindowMode mode);

	// Sets the window icon to be the given surface.
	void setWindowIcon(const Surface &icon);

	// Sets the window title.
	void setWindowTitle(const char *title);

	// Teleports the mouse to a location in the window.
	void warpMouse(int x, int y);

	// Sets the clip rectangle of the renderer so that pixels outside the specified area
	// will not be rendered. If rect is null, then clipping is disabled.
	void setClipRect(const SDL_Rect *rect);

	// Initialize the renderer for the game world. The "fullGameWindow" argument 
	// determines whether to render a "fullscreen" 3D image or just the part above 
	// the game interface. If there is an existing renderer in memory, it will be 
	// overwritten with the new one.
	void initializeWorldRendering(double resolutionScale, bool fullGameWindow,
		int renderThreadsMode);

	// Sets which mode to use for software render threads (low, medium, high, etc.).
	void setRenderThreadsMode(int mode);

	// Texture handle allocation functions.
	// @todo: see RendererSystem3D -- these should take TextureBuilders instead and return optional handles.
	bool tryCreateVoxelTexture(const TextureAssetReference &textureAssetRef, TextureManager &textureManager);
	bool tryCreateEntityTexture(const TextureAssetReference &textureAssetRef, bool flipped, bool reflective,
		TextureManager &textureManager);
	bool tryCreateSkyTexture(const TextureAssetReference &textureAssetRef, TextureManager &textureManager);
	bool tryCreateUiTexture(const BufferView2D<const uint32_t> &texels, UiTextureID *outID);
	bool tryCreateUiTexture(const BufferView2D<const uint8_t> &texels, const Palette &palette, UiTextureID *outID);
	bool tryCreateUiTexture(int width, int height, UiTextureID *outID);
	bool tryCreateUiTexture(TextureBuilderID textureBuilderID, PaletteID paletteID,
		const TextureManager &textureManager, UiTextureID *outID);

	// Allows for updating all texels in the given UI texture. Must be unlocked to flush the changes.
	uint32_t *lockUiTexture(UiTextureID textureID);
	void unlockUiTexture(UiTextureID textureID);

	// Texture handle freeing functions.
	// @todo: see RendererSystem3D -- these should take texture IDs instead.
	void freeVoxelTexture(const TextureAssetReference &textureAssetRef);
	void freeEntityTexture(const TextureAssetReference &textureAssetRef, bool flipped, bool reflective);
	void freeSkyTexture(const TextureAssetReference &textureAssetRef);
	void freeUiTexture(UiTextureID id);

	std::optional<Int2> tryGetUiTextureDims(UiTextureID id) const;

	// Helper methods for changing data in the 3D renderer.
	void setFogDistance(double fogDistance);
	void addChasmTexture(ArenaTypes::ChasmType chasmType, const uint8_t *colors,
		int width, int height, const Palette &palette);
	void setSky(const SkyInstance &skyInstance, const Palette &palette, TextureManager &textureManager);
	void setSkyColors(const uint32_t *colors, int count);
	void setNightLightsActive(bool active, const Palette &palette);
	void clearTextures();
	void clearSky();

	// Fills the native frame buffer with the draw color, or default black/transparent.
	void clear(const Color &color);
	void clear();
	void clearOriginal(const Color &color);
	void clearOriginal();

	// Wrapper methods for some SDL draw functions.
	void drawPixel(const Color &color, int x, int y);
	void drawLine(const Color &color, int x1, int y1, int x2, int y2);
	void drawRect(const Color &color, int x, int y, int w, int h);

	// Wrapper methods for some SDL fill functions.
	void fillRect(const Color &color, int x, int y, int w, int h);
	void fillOriginalRect(const Color &color, int x, int y, int w, int h);

	// Runs the 3D renderer which draws the world onto the native frame buffer.
	// If the renderer is uninitialized, this causes a crash.
	void renderWorld(const CoordDouble3 &eye, const Double3 &direction, double fovY, double ambient, double daytimePercent,
		double chasmAnimPercent, double latitude, bool nightLightsAreActive, bool isExterior, bool playerHasLight,
		int chunkDistance, double ceilingScale, const LevelInstance &levelInst, const SkyInstance &skyInst,
		const WeatherInstance &weatherInst, Random &random, const EntityDefinitionLibrary &entityDefLibrary,
		const Palette &palette);

	// Draw methods for the native and original frame buffers.
	void draw(const Texture &texture, int x, int y, int w, int h);
	void draw(const RendererSystem2D::RenderElement *renderElements, int count, RenderSpace renderSpace);

	// Refreshes the displayed frame buffer.
	void present();
};

#endif
