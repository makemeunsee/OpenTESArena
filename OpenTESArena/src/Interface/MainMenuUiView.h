#ifndef MAIN_MENU_UI_VIEW_H
#define MAIN_MENU_UI_VIEW_H

#include <string>

#include "../Assets/TextureAssetReference.h"
#include "../Math/Rect.h"
#include "../Math/Vector2.h"
#include "../Media/Color.h"
#include "../Media/TextureUtils.h"
#include "../Rendering/ArenaRenderUtils.h"
#include "../Rendering/RenderTextureUtils.h"
#include "../UI/ArenaFontName.h"
#include "../UI/TextAlignment.h"
#include "../UI/TextBox.h"

namespace MainMenuUiView
{
	Rect getLoadButtonRect();
	Rect getNewGameButtonRect();
	Rect getExitButtonRect();
	
	// Test values.
	Rect getTestButtonRect();
	constexpr TextureUtils::PatternType TestButtonPatternType = TextureUtils::PatternType::Custom1;
	const std::string TestButtonFontName = ArenaFontName::Arena;
	constexpr TextAlignment TestButtonTextAlignment = TextAlignment::MiddleCenter;

	Color getTestButtonTextColor();

	TextBox::InitInfo getTestButtonTextBoxInitInfo(const std::string_view &text, const FontLibrary &fontLibrary);
	TextBox::InitInfo getTestTypeTextBoxInitInfo(const FontLibrary &fontLibrary);
	TextBox::InitInfo getTestNameTextBoxInitInfo(const FontLibrary &fontLibrary);
	TextBox::InitInfo getTestWeatherTextBoxInitInfo(const FontLibrary &fontLibrary);

	Rect getTestTypeUpButtonRect();
	Rect getTestTypeDownButtonRect();
	Rect getTestIndexUpButtonRect();
	Rect getTestIndexDownButtonRect();
	Rect getTestIndex2UpButtonRect();
	Rect getTestIndex2DownButtonRect();
	Rect getTestWeatherUpButtonRect();
	Rect getTestWeatherDownButtonRect();

	TextureAssetReference getBackgroundTextureAssetRef();
	TextureAssetReference getPaletteTextureAssetRef();
	TextureAssetReference getTestArrowsTextureAssetRef();
	TextureAssetReference getTestArrowsPaletteTextureAssetRef();

	UiTextureID allocBackgroundTexture(TextureManager &textureManager, Renderer &renderer);
	UiTextureID allocTestArrowsTexture(TextureManager &textureManager, Renderer &renderer);
	UiTextureID allocTestButtonTexture(TextureManager &textureManager, Renderer &renderer);
}

#endif
