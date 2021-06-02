#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "SDL.h"

#include "FastTravelSubPanel.h"
#include "GameWorldPanel.h"
#include "ProvinceMapPanel.h"
#include "ProvinceMapUiController.h"
#include "ProvinceMapUiView.h"
#include "ProvinceSearchSubPanel.h"
#include "TextSubPanel.h"
#include "WorldMapPanel.h"
#include "../Assets/ArenaTextureName.h"
#include "../Assets/ArenaTypes.h"
#include "../Assets/CityDataFile.h"
#include "../Assets/ExeData.h"
#include "../Assets/IMGFile.h"
#include "../Assets/MIFFile.h"
#include "../Game/Game.h"
#include "../Game/Options.h"
#include "../Math/MathUtils.h"
#include "../Math/Random.h"
#include "../Math/Rect.h"
#include "../Media/TextureManager.h"
#include "../Rendering/ArenaRenderUtils.h"
#include "../Rendering/Renderer.h"
#include "../UI/CursorAlignment.h"
#include "../UI/FontLibrary.h"
#include "../UI/FontName.h"
#include "../UI/RichTextString.h"
#include "../UI/Surface.h"
#include "../UI/TextAlignment.h"
#include "../UI/TextBox.h"
#include "../UI/Texture.h"
#include "../WorldMap/LocationDefinition.h"
#include "../WorldMap/LocationInstance.h"
#include "../WorldMap/LocationUtils.h"

#include "components/debug/Debug.h"
#include "components/utilities/String.h"

ProvinceMapPanel::ProvinceMapPanel(Game &game, int provinceID, std::unique_ptr<ProvinceMapUiModel::TravelData> travelData)
	: Panel(game), travelData(std::move(travelData))
{
	this->searchButton = []()
	{
		const Rect &clickArea = ProvinceMapUiView::SearchButtonRect;
		return Button<Game&, ProvinceMapPanel&, int>(
			clickArea.getLeft(),
			clickArea.getTop(),
			clickArea.getWidth(),
			clickArea.getHeight(),
			ProvinceMapUiController::onSearchButtonSelected);
	}();

	this->travelButton = []()
	{
		const Rect &clickArea = ProvinceMapUiView::TravelButtonRect;
		return Button<Game&, ProvinceMapPanel&, bool>(
			clickArea.getLeft(),
			clickArea.getTop(),
			clickArea.getWidth(),
			clickArea.getHeight(),
			ProvinceMapUiController::onTravelButtonSelected);
	}();

	this->backToWorldMapButton = []()
	{
		const Rect &clickArea = ProvinceMapUiView::BackToWorldMapRect;
		return Button<Game&, std::unique_ptr<ProvinceMapUiModel::TravelData>>(
			clickArea.getLeft(),
			clickArea.getTop(),
			clickArea.getWidth(),
			clickArea.getHeight(),
			ProvinceMapUiController::onBackToWorldMapButtonSelected);
	}();

	this->provinceID = provinceID;
	this->blinkTimer = 0.0;

	// Get the palette for the background image.
	const TextureAssetReference provinceBgTextureAssetRef =
		ProvinceMapUiView::getProvinceBackgroundTextureAssetRef(game, provinceID);
	if (!IMGFile::tryExtractPalette(provinceBgTextureAssetRef.filename.c_str(), this->provinceMapPalette))
	{
		DebugCrash("Could not extract palette from \"" + provinceBgTextureAssetRef.filename + "\".");
	}

	// If displaying a province that contains a staff dungeon, get the staff dungeon icon's
	// raw palette indices (for yellow and red color swapping).
	const bool hasStaffDungeon = provinceID != LocationUtils::CENTER_PROVINCE_ID;
	if (hasStaffDungeon)
	{
		const std::string &cifName = ArenaTextureName::StaffDungeonIcons;
		this->staffDungeonCif = CIFFile();
		if (!this->staffDungeonCif.init(cifName.c_str()))
		{
			DebugCrash("Could not init .CIF file \"" + cifName + "\".");
		}
	}
}

void ProvinceMapPanel::trySelectLocation(int selectedLocationID)
{
	auto &game = this->getGame();
	auto &gameState = game.getGameState();
	const auto &binaryAssetLibrary = game.getBinaryAssetLibrary();

	const WorldMapDefinition &worldMapDef = gameState.getWorldMapDefinition();
	const ProvinceDefinition &currentProvinceDef = gameState.getProvinceDefinition();
	const LocationDefinition &currentLocationDef = gameState.getLocationDefinition();

	const ProvinceDefinition &selectedProvinceDef = worldMapDef.getProvinceDef(this->provinceID);
	const LocationDefinition &selectedLocationDef = selectedProvinceDef.getLocationDef(selectedLocationID);

	// Only continue if the selected location is not the player's current location.
	const bool matchesPlayerLocation = selectedProvinceDef.matches(currentProvinceDef) &&
		selectedLocationDef.matches(currentLocationDef);

	if (!matchesPlayerLocation)
	{
		// Set the travel data for the selected location and reset the blink timer.
		const Date &currentDate = gameState.getDate();

		// Use a copy of the RNG so displaying the travel pop-up multiple times doesn't
		// cause different day amounts.
		ArenaRandom tempRandom = gameState.getRandom();

		auto makeGlobalPoint = [](const LocationDefinition &locationDef, const ProvinceDefinition &provinceDef)
		{
			const Int2 localPoint(locationDef.getScreenX(), locationDef.getScreenY());
			return LocationUtils::getGlobalPoint(localPoint, provinceDef.getGlobalRect());
		};

		const Int2 srcGlobalPoint = makeGlobalPoint(currentLocationDef, currentProvinceDef);
		const Int2 dstGlobalPoint = makeGlobalPoint(selectedLocationDef, selectedProvinceDef);
		const int travelDays = LocationUtils::getTravelDays(srcGlobalPoint, dstGlobalPoint,
			currentDate.getMonth(), gameState.getWeathersArray(), tempRandom, binaryAssetLibrary);

		this->travelData = std::make_unique<ProvinceMapUiModel::TravelData>(
			selectedLocationID, this->provinceID, travelDays);
		this->blinkTimer = 0.0;

		// Create pop-up travel dialog.
		const std::string travelText = ProvinceMapUiModel::makeTravelText(this->getGame(), this->provinceID,
			currentLocationDef, currentProvinceDef, selectedLocationID, *this->travelData.get());
		std::unique_ptr<Panel> textPopUp = ProvinceMapUiModel::makeTextPopUp(game, travelText);
		game.pushSubPanel(std::move(textPopUp));
	}
	else
	{
		// Cannot travel to the player's current location. Create an error pop-up.
		const std::string &currentLocationName = [&gameState, &currentLocationDef]() -> const std::string&
		{
			const LocationInstance &currentLocationInst = gameState.getLocationInstance();
			return currentLocationInst.getName(currentLocationDef);
		}();

		const std::string errorText = ProvinceMapUiModel::makeAlreadyAtLocationText(game, currentLocationName);
		std::unique_ptr<Panel> textPopUp = ProvinceMapUiModel::makeTextPopUp(game, errorText);
		game.pushSubPanel(std::move(textPopUp));
	}
}

std::optional<Panel::CursorData> ProvinceMapPanel::getCurrentCursor() const
{
	return this->getDefaultCursor();
}

void ProvinceMapPanel::handleEvent(const SDL_Event &e)
{
	// Input will eventually depend on if the location pop-up is displayed, or
	// if a location is selected.
	auto &game = this->getGame();
	const auto &inputManager = game.getInputManager();
	const bool escapePressed = inputManager.keyPressed(e, SDLK_ESCAPE);
	const bool rightClick = inputManager.mouseButtonPressed(e, SDL_BUTTON_RIGHT);

	if (escapePressed || rightClick)
	{
		this->backToWorldMapButton.click(game, std::move(this->travelData));
	}

	const bool leftClick = inputManager.mouseButtonPressed(e, SDL_BUTTON_LEFT);

	if (leftClick)
	{
		const Int2 mousePosition = inputManager.getMousePosition();
		const Int2 originalPosition = game.getRenderer().nativeToOriginal(mousePosition);

		if (this->searchButton.contains(originalPosition))
		{
			this->searchButton.click(game, *this, this->provinceID);
		}
		else if (this->travelButton.contains(originalPosition))
		{
			this->travelButton.click(game, *this, this->travelData.get() != nullptr);
		}
		else if (this->backToWorldMapButton.contains(originalPosition))
		{
			this->backToWorldMapButton.click(game, std::move(this->travelData));
		}
		else
		{
			// The closest location to the cursor was clicked. See if it can be set as the
			// travel destination (depending on whether the player is already there).
			const int closestLocationID = this->getClosestLocationID(originalPosition);
			this->trySelectLocation(closestLocationID);
		}
	}
}

int ProvinceMapPanel::getClosestLocationID(const Int2 &originalPosition) const
{
	std::optional<Int2> closestPosition;

	// Lambda for comparing distances of two location points to the mouse position.
	auto closerThanCurrentClosest = [&originalPosition, &closestPosition](const Int2 &point)
	{
		if (!closestPosition.has_value())
		{
			return true;
		}

		const Int2 diff = point - originalPosition;
		const Int2 closestDiff = *closestPosition - originalPosition;
		// @todo: change to distance squared
		const double distance = std::sqrt((diff.x * diff.x) + (diff.y * diff.y));
		const double closestDistance = std::sqrt((closestDiff.x * closestDiff.x) + (closestDiff.y * closestDiff.y));
		return distance < closestDistance;
	};

	// Look through all visible locations to find the one closest to the mouse.
	std::optional<int> closestIndex;
	auto &game = this->getGame();
	auto &gameState = game.getGameState();
	const auto &binaryAssetLibrary = game.getBinaryAssetLibrary();

	const WorldMapInstance &worldMapInst = gameState.getWorldMapInstance();
	const ProvinceInstance &provinceInst = worldMapInst.getProvinceInstance(this->provinceID);
	const int provinceDefIndex = provinceInst.getProvinceDefIndex();
	const WorldMapDefinition &worldMapDef = gameState.getWorldMapDefinition();
	const ProvinceDefinition &provinceDef = worldMapDef.getProvinceDef(provinceDefIndex);

	for (int i = 0; i < provinceInst.getLocationCount(); i++)
	{
		const LocationInstance &locationInst = provinceInst.getLocationInstance(i);
		if (locationInst.isVisible())
		{
			const int locationDefIndex = locationInst.getLocationDefIndex();
			const LocationDefinition &locationDef = provinceDef.getLocationDef(locationDefIndex);
			const Int2 point(locationDef.getScreenX(), locationDef.getScreenY());

			if (closerThanCurrentClosest(point))
			{
				closestPosition = point;
				closestIndex = i;
			}
		}
	}

	DebugAssertMsg(closestIndex.has_value(), "No closest location ID found.");
	return *closestIndex;
}

void ProvinceMapPanel::tick(double dt)
{
	// Update the blink timer. Depending on which interval it's in, this will make the currently-selected
	// location (if any) have a red highlight.
	this->blinkTimer += dt;
}

void ProvinceMapPanel::handleFastTravel()
{
	// Switch to world map and push fast travel sub-panel on top of it.
	auto &game = this->getGame();
	game.pushSubPanel<FastTravelSubPanel>(game, *this->travelData.get());
	game.setPanel<WorldMapPanel>(game, std::move(this->travelData));
}

void ProvinceMapPanel::drawCenteredIcon(const Texture &texture, const Int2 &point, Renderer &renderer)
{
	const int x = point.x - (texture.getWidth() / 2);
	const int y = point.y - (texture.getHeight() / 2);
	renderer.drawOriginal(texture, x, y);
}

void ProvinceMapPanel::drawCenteredIcon(TextureBuilderID textureBuilderID, PaletteID paletteID,
	const Int2 &point, Renderer &renderer)
{
	const auto &textureManager = this->getGame().getTextureManager();
	const TextureBuilder &textureBuilder = textureManager.getTextureBuilderHandle(textureBuilderID);
	const int x = point.x - (textureBuilder.getWidth() / 2);
	const int y = point.y - (textureBuilder.getHeight() / 2);
	renderer.drawOriginal(textureBuilderID, paletteID, x, y, textureManager);
}

void ProvinceMapPanel::drawVisibleLocations(PaletteID backgroundPaletteID, TextureManager &textureManager,
	Renderer &renderer)
{
	const TextureAssetReference cityStateIconTextureAssetRef = ProvinceMapUiView::getCityStateIconTextureAssetRef();
	const TextureAssetReference townIconTextureAssetRef = ProvinceMapUiView::getTownIconTextureAssetRef();
	const TextureAssetReference villageIconTextureAssetRef = ProvinceMapUiView::getVillageIconTextureAssetRef();
	const TextureAssetReference dungeonIconTextureAssetRef = ProvinceMapUiView::getDungeonIconTextureAssetRef();	
	const std::optional<TextureBuilderID> cityStateIconTextureBuilderID = textureManager.tryGetTextureBuilderID(cityStateIconTextureAssetRef);
	const std::optional<TextureBuilderID> townIconTextureBuilderID = textureManager.tryGetTextureBuilderID(townIconTextureAssetRef);
	const std::optional<TextureBuilderID> villageIconTextureBuilderID = textureManager.tryGetTextureBuilderID(villageIconTextureAssetRef);
	const std::optional<TextureBuilderID> dungeonIconTextureBuilderID = textureManager.tryGetTextureBuilderID(dungeonIconTextureAssetRef);
	DebugAssert(cityStateIconTextureBuilderID.has_value());
	DebugAssert(townIconTextureBuilderID.has_value());
	DebugAssert(villageIconTextureBuilderID.has_value());
	DebugAssert(dungeonIconTextureBuilderID.has_value());

	auto &game = this->getGame();
	auto &gameState = game.getGameState();
	const auto &binaryAssetLibrary = game.getBinaryAssetLibrary();
	const WorldMapInstance &worldMapInst = gameState.getWorldMapInstance();
	const ProvinceInstance &provinceInst = worldMapInst.getProvinceInstance(this->provinceID);
	const int provinceDefIndex = provinceInst.getProvinceDefIndex();
	const WorldMapDefinition &worldMapDef = gameState.getWorldMapDefinition();
	const ProvinceDefinition &provinceDef = worldMapDef.getProvinceDef(provinceDefIndex);

	// Gets the displayed icon texture ID for a location.
	auto getLocationIconTextureID = [this, &textureManager, &renderer, &cityStateIconTextureBuilderID,
		&townIconTextureBuilderID, &villageIconTextureBuilderID,
		&dungeonIconTextureBuilderID](const LocationDefinition &locationDef) -> TextureBuilderID
	{
		switch (locationDef.getType())
		{
		case LocationDefinition::Type::City:
		{
			switch (locationDef.getCityDefinition().type)
			{
			case ArenaTypes::CityType::CityState:
				return *cityStateIconTextureBuilderID;
			case ArenaTypes::CityType::Town:
				return *townIconTextureBuilderID;
			case ArenaTypes::CityType::Village:
				return *villageIconTextureBuilderID;
			default:
				throw DebugException(std::to_string(static_cast<int>(locationDef.getCityDefinition().type)));
			}
		}
		case LocationDefinition::Type::Dungeon:
			return *dungeonIconTextureBuilderID;
		case LocationDefinition::Type::MainQuestDungeon:
		{
			const LocationDefinition::MainQuestDungeonDefinition::Type mainQuestDungeonType =
				locationDef.getMainQuestDungeonDefinition().type;

			if (mainQuestDungeonType == LocationDefinition::MainQuestDungeonDefinition::Type::Staff)
			{
				const TextureAssetReference staffDungeonIconTextureAssetRef =
					ProvinceMapUiView::getStaffDungeonIconTextureAssetRef(this->provinceID);
				const std::optional<TextureBuilderID> staffDungeonIconTextureBuilderID =
					textureManager.tryGetTextureBuilderID(staffDungeonIconTextureAssetRef);
				if (!staffDungeonIconTextureBuilderID.has_value())
				{
					DebugCrash("Couldn't get staff dungeon icon texture builder ID for \"" + staffDungeonIconTextureAssetRef.filename + "\".");
				}

				return *staffDungeonIconTextureBuilderID;
			}
			else
			{
				return *dungeonIconTextureBuilderID;
			}
		}
		default:
			throw DebugException(std::to_string(static_cast<int>(locationDef.getType())));
		}
	};

	auto drawIconIfVisible = [this, &textureManager, &renderer, &backgroundPaletteID, &provinceDef,
		&getLocationIconTextureID](const LocationInstance &locationInst)
	{
		if (locationInst.isVisible())
		{
			const int locationDefIndex = locationInst.getLocationDefIndex();
			const LocationDefinition &locationDef = provinceDef.getLocationDef(locationDefIndex);
			const Int2 point(locationDef.getScreenX(), locationDef.getScreenY());
			const TextureBuilderID iconTextureBuilderID = getLocationIconTextureID(locationDef);
			this->drawCenteredIcon(iconTextureBuilderID, backgroundPaletteID, point, renderer);
		}
	};

	// Draw all visible location icons in the province.
	for (int i = 0; i < provinceInst.getLocationCount(); i++)
	{
		const LocationInstance &locationInst = provinceInst.getLocationInstance(i);
		drawIconIfVisible(locationInst);
	}
}

void ProvinceMapPanel::drawLocationHighlight(const LocationDefinition &locationDef,
	LocationHighlightType highlightType, PaletteID highlightPaletteID, TextureManager &textureManager,
	Renderer &renderer)
{
	auto drawHighlightTextureBuilderID = [this, &locationDef, &renderer, highlightPaletteID](
		TextureBuilderID highlightTextureBuilderID)
	{
		const Int2 point(locationDef.getScreenX(), locationDef.getScreenY());
		this->drawCenteredIcon(highlightTextureBuilderID, highlightPaletteID, point, renderer);
	};

	auto drawHighlightTexture = [this, &locationDef, &renderer](const Texture &highlight)
	{
		const Int2 point(locationDef.getScreenX(), locationDef.getScreenY());
		this->drawCenteredIcon(highlight, point, renderer);
	};

	// Generic highlights (city, town, village, and dungeon).
	const std::string outlinesFilename = (highlightType == ProvinceMapPanel::LocationHighlightType::Current) ?
		ProvinceMapUiView::getMapIconOutlinesFilename() : ProvinceMapUiView::getMapIconBlinkingOutlinesFilename();

	const std::optional<TextureBuilderIdGroup> highlightTextureBuilderIDs =
		textureManager.tryGetTextureBuilderIDs(outlinesFilename.c_str());
	if (!highlightTextureBuilderIDs.has_value())
	{
		DebugLogError("Couldn't get highlight texture builder IDs for \"" + outlinesFilename + "\".");
		return;
	}

	auto handleCityHighlight = [&textureManager, &renderer, &locationDef, &drawHighlightTextureBuilderID,
		&highlightTextureBuilderIDs]()
	{
		const int highlightIndex = [&locationDef, &highlightTextureBuilderIDs]()
		{
			const LocationDefinition::CityDefinition &cityDef = locationDef.getCityDefinition();

			switch (cityDef.type)
			{
			case ArenaTypes::CityType::CityState:
				return ProvinceMapUiView::CityStateIconHighlightIndex;
			case ArenaTypes::CityType::Town:
				return ProvinceMapUiView::TownIconHighlightIndex;
			case ArenaTypes::CityType::Village:
				return ProvinceMapUiView::VillageIconHighlightIndex;
			default:
				DebugUnhandledReturnMsg(int, std::to_string(static_cast<int>(cityDef.type)));
			}
		}();

		const TextureBuilderID highlightTextureBuilderID = highlightTextureBuilderIDs->getID(highlightIndex);
		drawHighlightTextureBuilderID(highlightTextureBuilderID);
	};

	auto handleDungeonHighlight = [this, &textureManager, &drawHighlightTextureBuilderID,
		&highlightTextureBuilderIDs]()
	{
		// Named dungeon (they all use the same icon).
		const TextureBuilderID highlightTextureBuilderID =
			highlightTextureBuilderIDs->getID(ProvinceMapUiView::DungeonIconHighlightIndex);
		drawHighlightTextureBuilderID(highlightTextureBuilderID);
	};

	auto handleMainQuestDungeonHighlight = [this, &locationDef, highlightType, &textureManager,
		&renderer, &drawHighlightTextureBuilderID, &drawHighlightTexture, &highlightTextureBuilderIDs]()
	{
		const LocationDefinition::MainQuestDungeonDefinition &mainQuestDungeonDef =
			locationDef.getMainQuestDungeonDefinition();

		if (mainQuestDungeonDef.type == LocationDefinition::MainQuestDungeonDefinition::Type::Start)
		{
			// Start dungeon is not drawn.
		}
		else if (mainQuestDungeonDef.type == LocationDefinition::MainQuestDungeonDefinition::Type::Map)
		{
			// Staff map dungeon.
			const TextureBuilderID highlightTextureBuilderID =
				highlightTextureBuilderIDs->getID(ProvinceMapUiView::DungeonIconHighlightIndex);
			drawHighlightTextureBuilderID(highlightTextureBuilderID);
		}
		else if (mainQuestDungeonDef.type == LocationDefinition::MainQuestDungeonDefinition::Type::Staff)
		{
			// Staff dungeon. It changes a value in the palette to give the icon's background
			// its yellow color (since there are no highlight icons for staff dungeons).
			const Texture highlightTexture = [this, highlightType, &renderer]()
			{
				// Get the palette indices associated with the staff dungeon icon.
				const CIFFile &iconCif = this->staffDungeonCif;
				const int cifWidth = iconCif.getWidth(this->provinceID);
				const int cifHeight = iconCif.getHeight(this->provinceID);

				// Make a copy of the staff dungeon icon with changes based on which
				// pixels should be highlighted.
				Surface surface = Surface::createWithFormat(cifWidth, cifHeight,
					Renderer::DEFAULT_BPP, Renderer::DEFAULT_PIXELFORMAT);

				auto getColorFromIndex = [this, highlightType, &surface](int paletteIndex)
				{
					const auto &palette = this->provinceMapPalette;
					DebugAssertIndex(palette, paletteIndex);
					const Color &color = palette[paletteIndex];
					return surface.mapRGBA(color.r, color.g, color.b, color.a);
				};

				const uint32_t highlightColor = getColorFromIndex(
					(highlightType == ProvinceMapPanel::LocationHighlightType::Current) ?
					ProvinceMapUiView::YellowPaletteIndex : ProvinceMapUiView::RedPaletteIndex);

				// Convert each palette index to its equivalent 32-bit color, changing 
				// background indices to highlight indices as they are found.
				const uint8_t *srcPixels = iconCif.getPixels(this->provinceID);
				uint32_t *dstPixels = static_cast<uint32_t*>(surface.get()->pixels);
				const int pixelCount = surface.getWidth() * surface.getHeight();
				std::transform(srcPixels, srcPixels + pixelCount, dstPixels,
					[&getColorFromIndex, highlightColor](uint8_t srcPixel)
				{
					return (srcPixel == ProvinceMapUiView::BackgroundPaletteIndex) ?
						highlightColor : getColorFromIndex(srcPixel);
				});

				Texture texture = renderer.createTextureFromSurface(surface);
				return texture;
			}();

			drawHighlightTexture(highlightTexture);
		}
		else
		{
			DebugNotImplementedMsg(std::to_string(static_cast<int>(mainQuestDungeonDef.type)));
		}
	};

	// Decide how to highlight the location.
	switch (locationDef.getType())
	{
	case LocationDefinition::Type::City:
		handleCityHighlight();
		break;
	case LocationDefinition::Type::Dungeon:
		handleDungeonHighlight();
		break;
	case LocationDefinition::Type::MainQuestDungeon:
		handleMainQuestDungeonHighlight();
		break;
	default:
		DebugNotImplementedMsg(std::to_string(static_cast<int>(locationDef.getType())));
	}
}

void ProvinceMapPanel::drawLocationName(int locationID, Renderer &renderer)
{
	auto &game = this->getGame();
	const auto &fontLibrary = game.getFontLibrary();
	const RichTextString richText(
		ProvinceMapUiModel::getLocationName(game, this->provinceID, locationID),
		ProvinceMapUiView::LocationFontName,
		ProvinceMapUiView::LocationTextColor,
		ProvinceMapUiView::LocationTextAlignment,
		fontLibrary);

	const Int2 center = ProvinceMapUiView::getLocationCenterPoint(game, this->provinceID, locationID);
	const TextBox::ShadowData shadowData(
		ProvinceMapUiView::LocationTextShadowColor,
		ProvinceMapUiView::LocationTextShadowOffset);
	const TextBox textBox(center - Int2(0, 10), richText, &shadowData, fontLibrary, renderer);
	const Surface &textBoxSurface = textBox.getSurface();

	// Clamp to screen edges, with some extra space on the left and right.
	const Int2 clampedPosition = ProvinceMapUiView::getLocationTextClampedPosition(
		textBox.getX(), textBox.getY(), textBoxSurface.getWidth(), textBoxSurface.getHeight());
	renderer.drawOriginal(textBox.getTexture(), clampedPosition.x, clampedPosition.y);
}

void ProvinceMapPanel::drawButtonTooltip(const std::string &text, Renderer &renderer)
{
	const Texture tooltip = TextureUtils::createTooltip(text, this->getGame().getFontLibrary(), renderer);

	const auto &inputManager = this->getGame().getInputManager();
	const Int2 mousePosition = inputManager.getMousePosition();
	const Int2 originalPosition = renderer.nativeToOriginal(mousePosition);
	const int mouseX = originalPosition.x;
	const int mouseY = originalPosition.y;
	const int x = ((mouseX + 8 + tooltip.getWidth()) < ArenaRenderUtils::SCREEN_WIDTH) ?
		(mouseX + 8) : (mouseX - tooltip.getWidth());
	const int y = ((mouseY + tooltip.getHeight()) < ArenaRenderUtils::SCREEN_HEIGHT) ?
		mouseY : (mouseY - tooltip.getHeight());

	renderer.drawOriginal(tooltip, x, y);
}

void ProvinceMapPanel::render(Renderer &renderer)
{
	DebugAssert(this->getGame().gameStateIsActive());

	// Clear full screen.
	renderer.clear();

	// Draw province map background.
	auto &game = this->getGame();
	auto &textureManager = game.getTextureManager();
	const TextureAssetReference paletteTextureAssetRef =
		ProvinceMapUiView::getProvinceBackgroundPaletteTextureAssetRef(game, this->provinceID);
	const std::optional<PaletteID> backgroundPaletteID = textureManager.tryGetPaletteID(paletteTextureAssetRef);
	if (!backgroundPaletteID.has_value())
	{
		DebugLogError("Couldn't get background palette ID for \"" + paletteTextureAssetRef.filename + "\".");
		return;
	}

	const TextureAssetReference backgroundTextureAssetRef =
		ProvinceMapUiView::getProvinceBackgroundTextureAssetRef(game, this->provinceID);
	const std::optional<TextureBuilderID> mapBackgroundTextureBuilderID =
		textureManager.tryGetTextureBuilderID(backgroundTextureAssetRef);
	if (!mapBackgroundTextureBuilderID.has_value())
	{
		DebugLogError("Couldn't get map background texture builder ID for \"" + backgroundTextureAssetRef.filename + "\".");
		return;
	}

	renderer.drawOriginal(*mapBackgroundTextureBuilderID, *backgroundPaletteID, textureManager);

	// Draw visible location icons.
	this->drawVisibleLocations(*backgroundPaletteID, textureManager, renderer);

	const auto &gameState = game.getGameState();
	const WorldMapDefinition &worldMapDef = gameState.getWorldMapDefinition();
	const ProvinceDefinition &provinceDef = worldMapDef.getProvinceDef(this->provinceID);
	const ProvinceDefinition &playerProvinceDef = gameState.getProvinceDefinition();

	// If the player is in the current province, highlight their current location.
	if (provinceDef.matches(playerProvinceDef))
	{
		const LocationDefinition &locationDef = gameState.getLocationDefinition();
		const auto highlightType = ProvinceMapPanel::LocationHighlightType::Current;
		this->drawLocationHighlight(locationDef, highlightType, *backgroundPaletteID, textureManager, renderer);
	}

	// If there is a currently selected location in this province, draw its blinking highlight
	// if within the "blink on" interval.
	if ((this->travelData.get() != nullptr) && (this->travelData->provinceID == this->provinceID))
	{
		const double blinkInterval = std::fmod(this->blinkTimer, ProvinceMapUiView::BlinkPeriodSeconds);
		const double blinkPeriodPercent = blinkInterval / ProvinceMapUiView::BlinkPeriodSeconds;

		// See if the blink period percent lies within the "on" percent. Use less-than
		// to compare them so the on-state appears before the off-state.
		if (blinkPeriodPercent < ProvinceMapUiView::BlinkPeriodPercentOn)
		{
			const LocationDefinition &selectedLocationDef = provinceDef.getLocationDef(this->travelData->locationID);
			const auto highlightType = ProvinceMapPanel::LocationHighlightType::Selected;

			this->drawLocationHighlight(selectedLocationDef, highlightType, *backgroundPaletteID,
				textureManager, renderer);
		}
	}
}

void ProvinceMapPanel::renderSecondary(Renderer &renderer)
{
	// Draw the name of the location closest to the mouse cursor.
	const auto &inputManager = this->getGame().getInputManager();
	const Int2 mousePosition = inputManager.getMousePosition();
	const Int2 originalPosition = renderer.nativeToOriginal(mousePosition);
	const int closestLocationID = this->getClosestLocationID(originalPosition);
	this->drawLocationName(closestLocationID, renderer);

	// Draw a tooltip if the mouse is over a button.
	if (ProvinceMapUiView::SearchButtonRect.contains(originalPosition))
	{
		this->drawButtonTooltip(ProvinceMapUiModel::SearchButtonTooltip, renderer);
	}
	else if (ProvinceMapUiView::TravelButtonRect.contains(originalPosition))
	{
		this->drawButtonTooltip(ProvinceMapUiModel::TravelButtonTooltip, renderer);
	}
	else if (ProvinceMapUiView::BackToWorldMapRect.contains(originalPosition))
	{
		this->drawButtonTooltip(ProvinceMapUiModel::BackToWorldMapButtonTooltip, renderer);
	}
}
