/*
 * Copyright (C) 2012 Me and My Shadow
 *
 * This file is part of Me and My Shadow.
 *
 * Me and My Shadow is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Me and My Shadow is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Me and My Shadow.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LEVELEDITSELECT_H
#define LEVELEDITSELECT_H

#include "LevelSelect.h"
#include "GameState.h"
#include "GameObjects.h"
#include "Player.h"
#include "GUIObject.h"
#ifdef __APPLE__
#include <SDL_mixer/SDL_mixer.h>
#include <SDL_ttf/SDL_ttf.h>
#else
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_ttf.h>
#endif
#include <vector>
#include <string>

//This is the LevelEditSelect state, here you can select levelpacks and levels.
class LevelEditSelect :public LevelSelect{
private:
	//Pointer to the GUIObjectRoot of the levelselect main gui.
	GUIObject* levelEditGUIObjectRoot;
  
	//Pointer to the new levelpack textfield.
	GUIObject* levelpackName;
	
	//Pointer to the remove levelpack button.
	GUIButton* propertiesPack;
	//Pointer to the remove levelpack button.
	GUIButton* removePack;
	
	//Pointer to the move map button.
	GUIButton* move;
	//Pointer to the remove map button.
	GUIButton* remove;
	//Pointer to the edit map button.
	GUIButton* edit;
	
	//String that contains the name of the current levelpack.
	std::string packName;
	
	//Method that will create the GUI elements.
	//initial: Boolean if it is the first time the gui is created.
	void createGUI(bool initial);
	
	//Method that should be called when changing the current levelpack in an abnormal way.
	void changePack();
	
	//This method will show a popup with levelpack specific settings.
	//newPack: Boolean if it's a new levelpack.
	void packProperties(bool newPack);
	
	//This method will show an add level dialog.
	void addLevel();
	
	//This method will show an move level dialog.
	void moveLevel();
public:
	//Constructor.
	LevelEditSelect();
	//Destructor.
	~LevelEditSelect();
	
	//Inherited from LevelSelect.
	//change: Boolean if the levelpack changed, if not we only have to rearrange the numbers.
	void refresh(bool change=true);
	void selectNumber(unsigned int number,bool selected);
	
	//Inherited from GameState.
	void render();
	
	//Inherited from GameState.
	void resize();

	//Inherited from LevelSelect.
	void renderTooltip(unsigned int number,int dy);
	
	//GUI events will be handled here.
	void GUIEventCallback_OnEvent(std::string name,GUIObject* obj,int eventType);
};

#endif
