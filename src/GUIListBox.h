/*
 * Copyright (C) 2011-2012 Me and My Shadow
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

#ifndef GUILISTBOX_H
#define GUILISTBOX_H

#include "GUIObject.h"
#include "GUIScrollBar.h"

//GUIObject that displays a list.
//It extends GUIObject because it's a special GUIObject.
class GUIListBox:public GUIObject{
public:
	//Vector containing the entries of the list.
	std::vector<std::string> item;
	std::vector<SDL_Surface*> images;
	bool selectable;
	
	//Boolean if the listbox should send click events.
	bool clickEvents;
private:
	//Scrollbar used when there are more entries than fit on the screen.
	GUIScrollBar* scrollBar;

	int firstItemY;
	
	//Boolean if update for scrollbar is needed.
	bool updateScrollbar;
public:
	//Constructor.
	//left: The relative x location of the GUIListBox.
	//top: The relative y location of the GUIListBox.
	//witdh: The width of the GUIListBox.
	//height: The height of the GUIListBox.
	//enabled: Boolean if the GUIListBox is enabled or not.
	//visible: Boolean if the GUIListBox is visisble or not.
	GUIListBox(int left=0,int top=0,int width=0,int height=0,bool enabled=true,bool visible=true,int gravity=GUIGravityLeft);
	
	//Destructor
	~GUIListBox();
	
	//Method to remove all items and clear cache.
	void clearItems();
	
	//Method to add an item to the widget.
	//name: Text of the item.
	//image: Custom image for the widget, if NULL the image will be generated from name string.
	void addItem(std::string name, SDL_Surface* image=NULL);
	
	//Method to update an item in the widget.
	//index: index of the item.
	//newName: New text for the item.
	//newImage: New custom image for the widget, if NULL the image will be generated from newName string.
	void updateItem(int index, std::string newText, SDL_Surface* newImage=NULL);
	
	//Method used the get item names from the widget.
	//index: index of the item.
	std::string getItem(int index);
	
	//Method used to handle mouse and/or key events.
	//x: The x mouse location.
	//y: The y mouse location.
	//enabled: Boolean if the parent is enabled or not.
	//visible: Boolean if the parent is visible or not.
	//processed: Boolean if the event has been processed (by the parent) or not.
	//Returns: Boolean if the event is processed by the child.
	virtual bool handleEvents(int x=0,int y=0,bool enabled=true,bool visible=true,bool processed=false);
	//Method that will render the GUIListBox.
	//x: The x location to draw the GUIListBox. (x+left)
	//y: The y location to draw the GUIListBox. (y+top)
	virtual void render(int x=0,int y=0,bool draw=true);
};


//GUIObject that displays a list on only one line.
//Instead of clicking the entries of the list you iterate through them.
//It extends GUIObject because it's a special GUIObject.
class GUISingleLineListBox:public GUIObject{
public:
	//Vector containing the entries of the list.
	std::vector<std::pair<std::string,std::string> > item;
	
	//Integer used for the animation of the arrow.
	int animation;
public:
	//Constructor.
	//left: The relative x location of the GUIListBox.
	//top: The relative y location of the GUIListBox.
	//witdh: The width of the GUIListBox.
	//height: The height of the GUIListBox.
	//enabled: Boolean if the GUIListBox is enabled or not.
	//visible: Boolean if the GUIListBox is visisble or not.
	GUISingleLineListBox(int left=0,int top=0,int width=0,int height=0,bool enabled=true,bool visible=true,int gravity=GUIGravityLeft);

	//Method for adding an item to the list.
	//name: The name of the item.
	//label: The text that is displayed.
	void addItem(std::string name,std::string label="");

	//Method for adding items from a vector to the list.
	//items: Vector containing the items in pairs, first is the name, second the label.
	void addItems(std::vector<std::pair<std::string,std::string> > items);

	//Method for adding items from a vector to the list.
	//items: Vector containing the items, the name will also be used as label.
	void addItems(std::vector<std::string> items);

	//Method for retrieving the name of an item for a given index.
	//index: The index of the item, when -1 is entered the current one will be used.
	std::string getName(unsigned int index=-1);
	
	//Method used to handle mouse and/or key events.
	//x: The x mouse location.
	//y: The y mouse location.
	//enabled: Boolean if the parent is enabled or not.
	//visible: Boolean if the parent is visible or not.
	//processed: Boolean if the event has been processed (by the parent) or not.
	//Returns: Boolean if the event is processed by the child.
	virtual bool handleEvents(int x=0,int y=0,bool enabled=true,bool visible=true,bool processed=false);
	//Method that will render the GUIListBox.
	//x: The x location to draw the GUIListBox. (x+left)
	//y: The y location to draw the GUIListBox. (y+top)
	virtual void render(int x=0,int y=0,bool draw=true);
};

#endif
