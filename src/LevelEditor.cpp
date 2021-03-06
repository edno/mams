/*
 * Copyright (C) 2011-2013 Me and My Shadow
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

#include "GameState.h"
#include "Globals.h"
#include "Functions.h"
#include "FileManager.h"
#include "GameObjects.h"
#include "ThemeManager.h"
#include "LevelPack.h"
#include "LevelEditor.h"
#include "TreeStorageNode.h"
#include "POASerializer.h"
#include "GUIListBox.h"
#include "GUITextArea.h"
#include "GUIWindow.h"
#include "GUISpinBox.h"
#include "InputManager.h"
#include "StatisticsManager.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#endif

#include "libs/tinyformat/tinyformat.h"

using namespace std;

static int levelTime,levelRecordings;

//Array containing translateble block names
static const char* blockNames[TYPE_MAX]={
	__("Block"),__("Player Start"),__("Shadow Start"),
	__("Exit"),__("Shadow Block"),__("Spikes"),
	__("Checkpoint"),__("Swap"),__("Fragile"),
	__("Moving Block"),__("Moving Shadow Block"),__("Moving Spikes"),
	__("Teleporter"),__("Button"),__("Switch"),
	__("Conveyor Belt"),__("Shadow Conveyor Belt"),__("Notification Block"),__("Collectable"),__("Pushable")
};

//Array indicates if block is linkable
static const bool isLinkable[TYPE_MAX]={
	false,false,false,
	false,false,false,
	false,false,false,
	true,true,true,
	true,true,true,
	false,false,false,false
};

/////////////////LevelEditorActionsPopup/////////////////

class LevelEditorActionsPopup:private GUIEventCallback{
private:
	//The parent object.
	LevelEditor* parent;

	//The position and size of window.
	SDL_Rect rect;

	//Array containing the actions in this popup.
	GUIListBox* actions;

	//GUI image.
	SDL_Surface* bmGUI;

	//Pointer to the object the actions apply to.
	GameObject* target;

	//The behaviour names.
	vector<string> behaviour;
	//The fragile block states.
	vector<string> states;

public:
	SDL_Rect getRect(){
		return rect;
	}
	void dismiss(){
		//Remove the actionsPopup from the parent.
		if(parent!=NULL && parent->actionsPopup==this){
			parent->actionsPopup=NULL;
		}

		//And delete ourself.
		delete this;
	}
	SDL_Surface* createItem(const char* caption,int icon){
		//FIXME: Add some sort of caching?
		SDL_Color fg={0,0,0};
		SDL_Surface* tip=TTF_RenderUTF8_Blended(fontText,caption,fg);
		SDL_SetAlpha(tip,0,0xFF);
		//Create the surface, we add 16px to the width for an icon,
		//plus 8px for the border to make it looks better.
		SDL_Surface* item=SDL_CreateRGBSurface(SDL_SWSURFACE,tip->w+16+8,24,32,RMASK,GMASK,BMASK,AMASK);
		SDL_Rect itemRect={0,0,item->w,item->h};
		SDL_FillRect(item,&itemRect,0x00FFFFFF);
		itemRect.y=3;
		itemRect.h=16;
		SDL_FillRect(item,&itemRect,0xFFFFFFFF);
		//Draw the text on the item surface.
		applySurface(16+8,0,tip,item,NULL);

		//Check if we should draw an icon.
		if(icon>0){
			//Draw the check (or not).
			SDL_Rect r={0,0,16,16};
			r.x=((icon-1)%8)*16;
			r.y=((icon-1)/8)*16;
			applySurface(4,3,bmGUI,item,&r);
		}

		//Free the tip surface.
		SDL_FreeSurface(tip);

		//Update the height.
		rect.h+=24;
		//Check if we should update the width., 8px extra on the width is for four pixels spacing on either side.
		if(item->w+8>rect.w)
			rect.w=item->w+8;
		
		return item;
	}
	void updateItem(int index,const char* action,const char* caption,int icon=0){
		SDL_Surface* item=createItem(caption,icon);
		actions->updateItem(index,action,item);
	}
	void addItem(const char* action,const char* caption,int icon=0){
		SDL_Surface* item=createItem(caption,icon);
		actions->addItem(action,item);
	}
	LevelEditorActionsPopup(LevelEditor* parent, GameObject* target, int x=0, int y=0){
		this->parent=parent;
		this->target=target;
		//NOTE: The size gets set in the addItem method, height is already four to prevent a scrollbar.
		rect.w=0;
		rect.h=4;

		//Load the gui images.
		bmGUI=loadImage(getDataPath()+"gfx/gui.png");

		//Create the behaviour vector.
		behaviour.push_back(_("On"));
		behaviour.push_back(_("Off"));
		behaviour.push_back(_("Toggle"));

		//Create the states list.
		states.push_back(_("Complete"));
		states.push_back(_("One step"));
		states.push_back(_("Two steps"));
		states.push_back(_("Gone"));
		//TODO: The width should be based on the longest option.

		//Create default actions.
		//NOTE: Width and height are determined later on when the options are rendered.
		actions=new GUIListBox(0,0,0,0);
		actions->eventCallback=this;
		
		//Check if it's a block or not.
		if(target!=NULL)
			addBlockItems();
		else
			addLevelItems();

		//Now set the size of the GUIListBox.
		actions->width=rect.w;
		actions->height=rect.h;

		if(x>SCREEN_WIDTH-rect.w) x=SCREEN_WIDTH-rect.w;
		else if(x<0) x=0;
		if(y>SCREEN_HEIGHT-rect.h) y=SCREEN_HEIGHT-rect.h;
		else if(y<0) y=0;
		rect.x=x;
		rect.y=y;
	}

	void addBlockItems(){
		//Get the type of the target.
		int type=target->type;

		//Check if the block is selected or not.
		std::vector<GameObject*>::iterator it;
		it=find(parent->selection.begin(),parent->selection.end(),target);
		if(it!=parent->selection.end())
			addItem("Deselect",_("Deselect"));
		else
			addItem("Select",_("Select"));
		addItem("Delete",_("Delete"),8);
		//Determine what to do depending on the type.
		if(isLinkable[type]){
			//Check if it's a moving block type or trigger.
			if(type==TYPE_BUTTON || type==TYPE_SWITCH || type==TYPE_PORTAL){
				addItem("Link",_("Link"),8*3);
				addItem("Remove Links",_("Remove Links"));

				//Check if it's a portal, which contains a automatic option, and triggers a behaviour one.
				if(type==TYPE_PORTAL){
					addItem("Automatic",_("Automatic"),(target->getEditorProperty("automatic")=="1")?2:1);
				}else{
					//Get the current behaviour.
					int currentBehaviour=2;
					if(target->getEditorProperty("behaviour")=="on"){
						currentBehaviour=0;
					}else if(target->getEditorProperty("behaviour")=="off"){
						currentBehaviour=1;
					}

					addItem("Behaviour",behaviour[currentBehaviour].c_str());
				}
			}else{
				addItem("Path",_("Path"));
				addItem("Remove Path",_("Remove Path"));

				//FIXME: We use hardcoded indeces, if the order changes we have a problem.
				addItem("Activated",_("Activated"),(target->getEditorProperty("activated")=="1")?2:1);
				addItem("Looping",_("Looping"),(target->getEditorProperty("loop")=="1")?2:1);
			}
		}
		//Check for a conveyor belt.
		if(type==TYPE_CONVEYOR_BELT || type==TYPE_SHADOW_CONVEYOR_BELT){
			addItem("Activated",_("Activated"),(target->getEditorProperty("activated")=="1")?2:1);
			addItem("Speed",_("Speed"));
		}
		//Check if it's a fragile block.
		if(type==TYPE_FRAGILE){
			//Get the current state.
			int currentState=atoi(target->getEditorProperty("state").c_str());
			addItem("State",states[currentState].c_str());
		}
		//Check if it's a notification block.
		if(type==TYPE_NOTIFICATION_BLOCK)
			addItem("Message",_("Message"));
		//Finally add scripting to the bottom.
		addItem("Scripting",_("Scripting"),8*2+1);
	}

	void addLevelItems(){
		addItem("LevelSettings",_("Settings"),8*2);
		addItem("LevelScripting",_("Scripting"),8*2+1);
	}
	
	~LevelEditorActionsPopup(){
		if(actions)
			delete actions;
	}

	void render(){
		//Draw the actions.
		actions->render(rect.x,rect.y);
		
		//get mouse position
		int x,y;
		SDL_GetMouseState(&x,&y);
		SDL_Rect mouse={x,y,0,0};
	}
	void handleEvents(){
		//Check if a mouse is pressed outside the popup.
		int x,y;
		SDL_GetMouseState(&x,&y);
		SDL_Rect mouse={x,y,0,0};
		if(event.type==SDL_MOUSEBUTTONDOWN && !checkCollision(mouse,rect)){
			dismiss();
			return;
		}
		//Let the listbox handle its events.
		actions->handleEvents(rect.x,rect.y);
	}
	void GUIEventCallback_OnEvent(std::string name,GUIObject* obj,int eventType){
		//NOTE: There should only be one GUIObject, so we know what event is fired.
		//Get the selected entry.
		std::string action=actions->item[actions->value];
		if(action=="Select"){
			//Add the target to the selection.
			parent->selection.push_back(target);
			dismiss();
			return;
		}else if(action=="Deselect"){
			//Check if the block is in the selection.
			std::vector<GameObject*>::iterator it;
			it=find(parent->selection.begin(),parent->selection.end(),target);
			if(it!=parent->selection.end()){
				//Remove the object from the selection.
				parent->selection.erase(it);
			}
			
			dismiss();
			return;
		}else if(action=="Delete"){
			parent->removeObject(target);
			dismiss();
			return;
		}else if(action=="Link"){
			parent->linking=true;
			parent->linkingTrigger=dynamic_cast<Block*>(target);
			parent->tool=LevelEditor::SELECT;
			dismiss();
			return;
		}else if(action=="Remove Links"){
			//Remove all the 
			std::map<Block*,vector<GameObject*> >::iterator it;
			it=parent->triggers.find(dynamic_cast<Block*>(target));
			if(it!=parent->triggers.end()){
				//Remove the targets.
				(*it).second.clear();
			}
			
			//In case of a portal remove its destination field.
			if(target->type==TYPE_PORTAL){
				target->setEditorProperty("destination","");
			}else{
				//We give the trigger a new id to prevent activating unlinked targets.
				char s[64];
				sprintf(s,"%u",parent->currentId);
				parent->currentId++;
				target->setEditorProperty("id",s);
			}
			dismiss();
			return;
		}else if(action=="Path"){
			parent->moving=true;
			parent->movingBlock=dynamic_cast<Block*>(target);
			parent->tool=LevelEditor::SELECT;
			dismiss();
			return;
		}else if(action=="Remove Path"){
			//Set the number of moving positions to zero.
			target->setEditorProperty("MovingPosCount","0");

			std::map<Block*,vector<MovingPosition> >::iterator it;
			it=parent->movingBlocks.find(dynamic_cast<Block*>(target));
			if(it!=parent->movingBlocks.end()){
				(*it).second.clear();
			}
			dismiss();
			return;
		}else if(action=="Message"){
			//Create the GUI.
			GUIWindow* root=new GUIWindow((SCREEN_WIDTH-600)/2,(SCREEN_HEIGHT-250)/2,600,250,true,true,_("Notification block"));
			root->name="notificationBlockWindow";
			root->eventCallback=parent;
			GUIObject* obj;

			obj=new GUILabel(40,50,240,36,_("Enter message here:"));
			root->addChild(obj);
			GUITextArea* textarea=new GUITextArea(50,90,500,100);
			//Set the name of the text area, which is used to identify the object later on.
			textarea->name="message";
			string tmp=target->getEditorProperty("message");
			//Change \n with the characters '\n'.
			while(tmp.find("\\n")!=string::npos){
				tmp=tmp.replace(tmp.find("\\n"),2,"\n");
			}
			textarea->setString(tmp);
			root->addChild(textarea);

			obj=new GUIButton(root->width*0.3,250-44,-1,36,_("OK"),0,true,true,GUIGravityCenter);
			obj->name="cfgNotificationBlockOK";
			obj->eventCallback=root;
			root->addChild(obj);
			obj=new GUIButton(root->width*0.7,250-44,-1,36,_("Cancel"),0,true,true,GUIGravityCenter);
			obj->name="cfgCancel";
			obj->eventCallback=root;
			root->addChild(obj);

			//Add the window to the GUIObjectRoot and the objectWindows map.
			GUIObjectRoot->addChild(root);
			parent->objectWindows[root]=target;
			
			//And dismiss this popup.
			dismiss();
			return;
		}else if(action=="Activated"){
			//Get the previous state.
			bool enabled=(target->getEditorProperty("activated")=="1");

			//Switch the state.
			enabled=!enabled;

			target->setEditorProperty("activated",enabled?"1":"0");

			updateItem(actions->value,"Enabled",_("Enabled"),enabled?2:1);
			actions->value=-1;
			return;
		}else if(action=="Looping"){
			//Get the previous state.
			bool loop=(target->getEditorProperty("loop")=="1");

			//Switch the state.
			loop=!loop;
			target->setEditorProperty("loop",loop?"1":"0");

			updateItem(actions->value,"Looping",_("Looping"),loop?2:1);
			actions->value=-1;
			return;
		}else if(action=="Automatic"){
			//Get the previous state.
			bool automatic=(target->getEditorProperty("automatic")=="1");

			//Switch the state.
			automatic=!automatic;
			target->setEditorProperty("automatic",automatic?"1":"0");

			updateItem(actions->value,"Automatic",_("Automatic"),automatic?2:1);
			actions->value=-1;
			return;
		}else if(action=="Behaviour"){
			//Get the current behaviour.
			int currentBehaviour=2;
			string behave=target->getEditorProperty("behaviour");
			if(behave=="on"){
				currentBehaviour=0;
			}else if(behave=="off"){
				currentBehaviour=1;
			}

			//Increase the behaviour.
			currentBehaviour++;
			if(currentBehaviour>2)
				currentBehaviour=0;

			//Update the data of the block.
			target->setEditorProperty("behaviour",behaviour[currentBehaviour]);

			//And update the item.
			updateItem(actions->value,"Behaviour",behaviour[currentBehaviour].c_str());
			actions->value=-1;
			return;
		}else if(action=="State"){
			//Get the current state.
			int currentState=atoi(target->getEditorProperty("state").c_str());

			//Increase the state.
			currentState++;
			if(currentState>3)
				currentState=0;

			//Update the data of the block.
			char s[64];
			sprintf(s,"%d",currentState);
			target->setEditorProperty("state",s);

			//And update the item.
			updateItem(actions->value,"State",states[currentState].c_str());
			actions->value=-1;
			return;
		}else if(action=="Speed"){
			//Create the GUI.
			GUIWindow* root=new GUIWindow((SCREEN_WIDTH-600)/2,(SCREEN_HEIGHT-250)/2,600,250,true,true,_("Conveyor belt speed"));
			root->name="conveyorBlockWindow";
			root->eventCallback=parent;
			GUIObject* obj;

			obj=new GUILabel(40,100,240,36,_("Enter speed here:"));
			root->addChild(obj);
			GUISpinBox* obj2=new GUISpinBox(240,100,320,36);
			//Set the name of the text area, which is used to identify the object later on.
			obj2->name="speed";
			obj2->caption=target->getEditorProperty("speed");
			obj2->update();
			root->addChild(obj2);
				
			obj=new GUIButton(root->width*0.3,250-44,-1,36,_("OK"),0,true,true,GUIGravityCenter);
			obj->name="cfgConveyorBlockOK";
			obj->eventCallback=root;
			root->addChild(obj);
			obj=new GUIButton(root->width*0.7,250-44,-1,36,_("Cancel"),0,true,true,GUIGravityCenter);
			obj->name="cfgCancel";
			obj->eventCallback=root;
			root->addChild(obj);

			//Add the window to the GUIObjectRoot and the objectWindows map.
			GUIObjectRoot->addChild(root);
			parent->objectWindows[root]=target;
			
			//And dismiss this popup.
			dismiss();
			return;
		}else if(action=="Scripting"){
			//Create the GUI.
			GUIWindow* root=new GUIWindow((SCREEN_WIDTH-600)/2,(SCREEN_HEIGHT-500)/2,600,500,true,true,_("Scripting"));
			root->name="scriptingWindow";
			root->eventCallback=parent;
			GUIObject* obj;

			obj=new GUILabel(50,60,240,36,_("Id:"));
			root->addChild(obj);

			obj=new GUITextBox(100,60,240,36,dynamic_cast<Block*>(target)->id.c_str());
			obj->name="id";
			root->addChild(obj);

			GUISingleLineListBox* list=new GUISingleLineListBox(50,100,500,36);
			std::map<std::string,int>::iterator it;
			for(it=Game::gameObjectEventNameMap.begin();it!=Game::gameObjectEventNameMap.end();++it)
				list->addItem(it->first);
			list->name="cfgScriptingEventType";
			list->value=0;
			list->eventCallback=root;
			root->addChild(list);

			//Add a text area for each event type.
			Block* block=dynamic_cast<Block*>(target);
			for(unsigned int i=0;i<list->item.size();i++){
				GUITextArea* text=new GUITextArea(50,140,500,300);
				text->name=list->item[i].first;
				text->setFont(fontMono);
				//Only set the first one visible and enabled.
				text->visible=(i==0);
				text->enabled=(i==0);

				map<int,string>::iterator it=block->scripts.find(Game::gameObjectEventNameMap[list->item[i].first]);
				if(it!=block->scripts.end())
					text->setString(it->second);

				root->addChild(text);
			}


			obj=new GUIButton(root->width*0.3,500-44,-1,36,_("OK"),0,true,true,GUIGravityCenter);
			obj->name="cfgScriptingOK";
			obj->eventCallback=root;
			root->addChild(obj);
			obj=new GUIButton(root->width*0.7,500-44,-1,36,_("Cancel"),0,true,true,GUIGravityCenter);
			obj->name="cfgCancel";
			obj->eventCallback=root;
			root->addChild(obj);

			//Add the window to the GUIObjectRoot and the objectWindows map.
			GUIObjectRoot->addChild(root);
			parent->objectWindows[root]=target;

			//And dismiss this popup.
			dismiss();
			return;
		}else if(action=="LevelSettings"){
			//Open the levelSettings window.
			parent->levelSettings();
			
			//And dismiss this popup.
			dismiss();
			return;
		}else if(action=="LevelScripting"){
			//Create the GUI.
			GUIWindow* root=new GUIWindow((SCREEN_WIDTH-600)/2,(SCREEN_HEIGHT-500)/2,600,500,true,true,_("Level Scripting"));
			root->name="levelScriptingWindow";
			root->eventCallback=parent;
			GUIObject* obj;

			GUISingleLineListBox* list=new GUISingleLineListBox(50,60,500,36);
			std::map<std::string,int>::iterator it;
			for(it=Game::levelEventNameMap.begin();it!=Game::levelEventNameMap.end();++it)
				list->addItem(it->first);
			list->name="cfgLevelScriptingEventType";
			list->value=0;
			list->eventCallback=root;
			root->addChild(list);

			//Add a text area for each event type.
			for(unsigned int i=0;i<list->item.size();i++){
				GUITextArea* text=new GUITextArea(50,100,500,340);
				text->name=list->item[i].first;
				text->setFont(fontMono);
				//Only set the first one visible and enabled.
				text->visible=(i==0);
				text->enabled=(i==0);

				map<int,string>::iterator it=parent->scripts.find(Game::levelEventNameMap[list->item[i].first]);
				if(it!=parent->scripts.end())
					text->setString(it->second);

				root->addChild(text);
			}


			obj=new GUIButton(root->width*0.3,500-44,-1,36,_("OK"),0,true,true,GUIGravityCenter);
			obj->name="cfgLevelScriptingOK";
			obj->eventCallback=root;
			root->addChild(obj);
			obj=new GUIButton(root->width*0.7,500-44,-1,36,_("Cancel"),0,true,true,GUIGravityCenter);
			obj->name="cfgCancel";
			obj->eventCallback=root;
			root->addChild(obj);

			//Add the window to the GUIObjectRoot and the objectWindows map.
			GUIObjectRoot->addChild(root);
			parent->objectWindows[root]=target;

			//And dismiss this popup.
			dismiss();
			return;
		}
	}
};


/////////////////LevelEditorSelectionPopup/////////////////

class LevelEditorSelectionPopup{
private:
	//The parent object
	LevelEditor* parent;

	//The position of window
	SDL_Rect rect;

	//GUI image
	SDL_Surface* bmGUI;

	//The selection
	std::vector<GameObject*> selection;

	//The scrollbar
	GUIScrollBar* scrollBar;

	//Highlighted object
	GameObject* highlightedObj;

	//Highlighted button index. 0=none 1=select/deselect 2=delete 3=configure
	int highlightedBtn;
public:
	int startRow,showedRow;

	//If selection is dirty
	bool dirty;

public:
	SDL_Rect getRect(){
		return rect;
	}
	int width(){
		return rect.w;
	}
	int height(){
		return rect.h;
	}
	void updateScrollBar(){
		int m=selection.size()-showedRow;
		if(m>0){
			if(startRow<0) startRow=0;
			else if(startRow>m) startRow=m;

			if(scrollBar==NULL){
				scrollBar=new GUIScrollBar(0,0,16,rect.h-16,ScrollBarVertical,startRow,0,m,1,showedRow);
			}

			scrollBar->visible=true;
			scrollBar->maxValue=m;
			scrollBar->value=startRow;
		}else{
			startRow=0;
			if(scrollBar){
				scrollBar->visible=false;
				scrollBar->value=0;
			}
		}
	}
	void updateSelection(){
		if(parent!=NULL){
			std::vector<Block*>& v=parent->levelObjects;

			for(int i=selection.size()-1;i>=0;i--){
				if(find(v.begin(),v.end(),selection[i])==v.end()){
					selection.erase(selection.begin()+i);
				}
			}

			updateScrollBar();
		}
	}
	void dismiss(){
		if(parent!=NULL && parent->selectionPopup==this){
			parent->selectionPopup=NULL;
		}
		delete this;
	}
	LevelEditorSelectionPopup(LevelEditor* parent, std::vector<GameObject*>& selection, int x=0, int y=0){
		this->parent=parent;
		this->selection=selection;

		dirty=false;
		scrollBar=NULL;
		highlightedObj=NULL;
		highlightedBtn=0;

		//calc window size
		startRow=0;
		showedRow=selection.size();
		int m=SCREEN_HEIGHT/64-1;
		if(showedRow>m) showedRow=m;

		rect.w=320;
		rect.h=showedRow*64+16;

		if(x>SCREEN_WIDTH-rect.w) x=SCREEN_WIDTH-rect.w;
		else if(x<0) x=0;
		if(y>SCREEN_HEIGHT-rect.h) y=SCREEN_HEIGHT-rect.h;
		else if(y<0) y=0;
		rect.x=x;
		rect.y=y;

		updateScrollBar();

		//Load the gui images.
		bmGUI=loadImage(getDataPath()+"gfx/gui.png");
	}
	~LevelEditorSelectionPopup(){
		if(scrollBar)
			delete scrollBar;
	}
	void move(int x,int y){
		if(x>SCREEN_WIDTH-rect.w) x=SCREEN_WIDTH-rect.w;
		else if(x<0) x=0;
		if(y>SCREEN_HEIGHT-rect.h) y=SCREEN_HEIGHT-rect.h;
		else if(y<0) y=0;
		rect.x=x;
		rect.y=y;
	}
	void render(){
		//Check dirty
		if(dirty){
			updateSelection();
			if(selection.empty()){
				dismiss();
				return;
			}
			dirty=false;
		}

		//background
		drawGUIBox(rect.x,rect.y,rect.w,rect.h,screen,0xFFFFFFFFU);

		//get mouse position
		int x,y;
		SDL_GetMouseState(&x,&y);
		SDL_Rect mouse={x,y,0,0};

		//the tool tip of item
		SDL_Rect tooltipRect;
		string tooltip;

		if(scrollBar && scrollBar->visible){
			startRow=scrollBar->value;
		}

		highlightedObj=NULL;
		highlightedBtn=0;

		//draw avaliable item
		for(int i=0;i<showedRow;i++){
			int j=startRow+i;
			if(j>=(int)selection.size()) break;

			SDL_Rect r={rect.x+8,rect.y+i*64+8,rect.w-16,64};
			if(scrollBar && scrollBar->visible) r.w-=24;

			//check highlight
			if(checkCollision(mouse,r)){
				highlightedObj=selection[j];
				SDL_FillRect(screen,&r,0xCCCCCC);
			}

			int type=selection[j]->type;

			//draw tile picture
			ThemeBlock* obj=objThemes.getBlock(type);
			if(obj){
				obj->editorPicture.draw(screen,r.x+7,r.y+7);
			}

			//draw name
			SDL_Color fg={0,0,0};
			SDL_Surface* txt=TTF_RenderUTF8_Blended(fontText,_(blockNames[type]),fg);
			if(txt!=NULL){
				SDL_Rect r2={r.x+64,r.y+(64-txt->h)/2,0,0};
				SDL_BlitSurface(txt,NULL,screen,&r2);
				SDL_FreeSurface(txt);
			}

			if(parent!=NULL){
				//draw selected
				{
					std::vector<GameObject*> &v=parent->selection;
					bool isSelected=find(v.begin(),v.end(),selection[j])!=v.end();

					SDL_Rect r1={isSelected?16:0,0,16,16};
					SDL_Rect r2={r.x+r.w-72,r.y+20,24,24};
					if(checkCollision(mouse,r2)){
						drawGUIBox(r2.x,r2.y,r2.w,r2.h,screen,0x999999FFU);
						tooltipRect=r2;
						tooltip=_("Select");
						highlightedBtn=1;
					}
					r2.x+=4;
					r2.y+=4;
					SDL_BlitSurface(bmGUI,&r1,screen,&r2);
				}

				//draw delete
				{
					SDL_Rect r1={112,0,16,16};
					SDL_Rect r2={r.x+r.w-48,r.y+20,24,24};
					if(checkCollision(mouse,r2)){
						drawGUIBox(r2.x,r2.y,r2.w,r2.h,screen,0x999999FFU);
						tooltipRect=r2;
						tooltip=_("Delete");
						highlightedBtn=2;
					}
					r2.x+=4;
					r2.y+=4;
					SDL_BlitSurface(bmGUI,&r1,screen,&r2);
				}

				//draw configure
				{
					SDL_Rect r1={112,16,16,16};
					SDL_Rect r2={r.x+r.w-24,r.y+20,24,24};
					if(checkCollision(mouse,r2)){
						drawGUIBox(r2.x,r2.y,r2.w,r2.h,screen,0x999999FFU);
						tooltipRect=r2;
						tooltip=_("Configure");
						highlightedBtn=3;
					}
					r2.x+=4;
					r2.y+=4;
					SDL_BlitSurface(bmGUI,&r1,screen,&r2);
				}
			}
		}

		//draw scrollbar
		if(scrollBar && scrollBar->visible){
			scrollBar->render(rect.x+rect.w-24,rect.y+8);
		}

		//draw tooltip
		if(!tooltip.empty()){
			//The back and foreground colors.
			SDL_Color fg={0,0,0};

			//Tool specific text.
			SDL_Surface* tip=TTF_RenderUTF8_Blended(fontText,tooltip.c_str(),fg);

			//Draw only if there's a tooltip available
			if(tip!=NULL){
				tooltipRect.y-=4;
				tooltipRect.h+=8;
				if(tooltipRect.y+tooltipRect.h+tip->h>SCREEN_HEIGHT-20)
					tooltipRect.y-=tip->h;
				else
					tooltipRect.y+=tooltipRect.h;

				if(tooltipRect.x+tip->w>SCREEN_WIDTH-20)
					tooltipRect.x=SCREEN_WIDTH-20-tip->w;

				//Draw borders around text
				Uint32 color=0xFFFFFF00|230;
				drawGUIBox(tooltipRect.x-2,tooltipRect.y-2,tip->w+4,tip->h+4,screen,color);

				//Draw tooltip's text
				SDL_BlitSurface(tip,NULL,screen,&tooltipRect);
				SDL_FreeSurface(tip);
			}
		}
	}
	void handleEvents(){
		//Check dirty
		if(dirty){
			updateSelection();
			if(selection.empty()){
				dismiss();
				return;
			}
			dirty=false;
		}

		//Check scrollbar event
		if(scrollBar && scrollBar->visible){
			if(scrollBar->handleEvents(rect.x+rect.w-24,rect.y+8)) return;
		}

		if(event.type==SDL_MOUSEBUTTONDOWN){
			//check mousewheel
			if(event.button.button==SDL_BUTTON_WHEELUP){
				startRow-=2;
				updateScrollBar();
				return;
			}else if(event.button.button==SDL_BUTTON_WHEELDOWN){
				startRow+=2;
				updateScrollBar();
				return;
			}else if(event.button.button==SDL_BUTTON_LEFT){
				SDL_Rect mouse={event.button.x,event.button.y,0,0};
				
				//Check if close it
				if(!checkCollision(mouse,rect)){
					dismiss();
					return;
				}

				//Check if item is clicked
				if(highlightedObj!=NULL && highlightedBtn>0 && parent!=NULL){
					std::vector<Block*>& v=parent->levelObjects;
					
					if(find(v.begin(),v.end(),highlightedObj)!=v.end()){
						switch(highlightedBtn){
						case 1:
							{
								std::vector<GameObject*>& v2=parent->selection;
								std::vector<GameObject*>::iterator it=find(v2.begin(),v2.end(),highlightedObj);
							
								if(it==v2.end()){
									v2.push_back(highlightedObj);
								}else{
									v2.erase(it);
								}
							}
							break;
						case 2:
							parent->removeObject(highlightedObj);
							break;
						case 3:
							if(parent->actionsPopup)
								delete parent->actionsPopup;
							parent->actionsPopup=new LevelEditorActionsPopup(parent,highlightedObj,mouse.x,mouse.y);
							break;
						}
					}
				}
			}
		}
	}
};

/////////////////MovingPosition////////////////////////////
MovingPosition::MovingPosition(int x,int y,int time){
	this->x=x;
	this->y=y;
	this->time=time;
}

MovingPosition::~MovingPosition(){}

void MovingPosition::updatePosition(int x,int y){
	this->x=x;
	this->y=y;
}

/////////////////LEVEL EDITOR//////////////////////////////
LevelEditor::LevelEditor():Game(){
	//Get the target time and recordings.
	levelTime=levels->getLevel()->targetTime;
	levelRecordings=levels->getLevel()->targetRecordings;

	//This will set some default settings.
	reset();

	//Create the GUI root.
	GUIObjectRoot=new GUIObject(0,0,SCREEN_WIDTH,SCREEN_HEIGHT);

	//Load the toolbar.
	toolbar=loadImage(getDataPath()+"gfx/menu/toolbar.png");
	SDL_Rect tmp={(SCREEN_WIDTH-410)/2,SCREEN_HEIGHT-50,410,50};
	toolbarRect=tmp;
	
	selectionPopup=NULL;
	actionsPopup=NULL;
	
	movingSpeedWidth=-1;

	//Load the selectionMark.
	selectionMark=loadImage(getDataPath()+"gfx/menu/selection.png");

	//Load the movingMark.
	movingMark=loadImage(getDataPath()+"gfx/menu/moving.png");

	//Load the gui images.
	bmGUI=loadImage(getDataPath()+"gfx/gui.png");

	//Create the semi transparent surface.
	placement=SDL_CreateRGBSurface(SDL_SWSURFACE|SDL_SRCALPHA,SCREEN_WIDTH,SCREEN_HEIGHT,32,RMASK,GMASK,BMASK,0);
	SDL_SetColorKey(placement,SDL_SRCCOLORKEY|SDL_RLEACCEL,SDL_MapRGB(placement->format,255,0,255));
	SDL_SetAlpha(placement,SDL_SRCALPHA,125);

	//Count the level editing time.
	statsMgr.startLevelEdit();
}

LevelEditor::~LevelEditor(){
	//Loop through the levelObjects and delete them.
	for(unsigned int i=0;i<levelObjects.size();i++)
		delete levelObjects[i];
	levelObjects.clear();
	selection.clear();

	//Free the placement surface.
	SDL_FreeSurface(placement);

	//Delete the GUI.
	if(GUIObjectRoot){
		delete GUIObjectRoot;
		GUIObjectRoot=NULL;
	}

	//Delete the popup
	if(selectionPopup){
		delete selectionPopup;
		selectionPopup=NULL;
	}

	//Delete the popup
	if(actionsPopup){
		delete actionsPopup;
		actionsPopup=NULL;
	}

	//Reset the camera.
	camera.x=0;
	camera.y=0;

	//Count the level editing time.
	statsMgr.endLevelEdit();
}

void LevelEditor::reset(){
	//Set some default values.
	playMode=false;
	tool=ADD;
	currentType=0;
	toolboxVisible=false;
	toolboxRect.x=-1;
	toolboxRect.y=-1;
	toolboxRect.w=0;
	toolboxRect.h=0;
	toolboxIndex=0;
	pressedShift=false;
	pressedLeftMouse=false;
	dragging=false;
	selectionDrag=false;
	dragCenter=NULL;
	cameraXvel=0;
	cameraYvel=0;
	linking=false;
	linkingTrigger=NULL;
	currentId=0;
	movingBlock=NULL;
	moving=false;
	movingSpeed=10;
	tooltip=-1;

	//Set the player and shadow to their starting position.
	player.setLocation(player.fx,player.fy);
	shadow.setLocation(shadow.fx,shadow.fy);

	selection.clear();
	clipboard.clear();
	triggers.clear();
	movingBlocks.clear();

	//Delete any gui.
	if(GUIObjectRoot){
		delete GUIObjectRoot;
		GUIObjectRoot=NULL;
	}
	//Clear the GUIWindow object map.
	objectWindows.clear();
}

void LevelEditor::loadLevelFromNode(TreeStorageNode* obj, const std::string& fileName){
	//call the method of base class.
	Game::loadLevelFromNode(obj,fileName);

	//now do our own stuff.
	string s=editorData["time"];
	if(s.empty() || !(s[0]>='0' && s[0]<='9')){
		levelTime=-1;
	}else{
		levelTime=atoi(s.c_str());
	}

	s=editorData["recordings"];
	if(s.empty() || !(s[0]>='0' && s[0]<='9')){
		levelRecordings=-1;
	}else{
		levelRecordings=atoi(s.c_str());
	}

	//NOTE: We set the camera here since we know the dimensions of the level.
	if(LEVEL_WIDTH<SCREEN_WIDTH)
		camera.x=-(SCREEN_WIDTH-LEVEL_WIDTH)/2;
	else
		camera.x=0;
	if(LEVEL_HEIGHT<SCREEN_HEIGHT)
		camera.y=-(SCREEN_HEIGHT-LEVEL_HEIGHT)/2;
	else
		camera.y=0;

	//The level is loaded, so call postLoad.
	postLoad();
}

void LevelEditor::saveLevel(string fileName){
	//Create the output stream and check if it starts.
	std::ofstream save(fileName.c_str());
	if(!save) return;

	//The dimensions of the level.
	int maxX=0;
	int maxY=0;

	//The storageNode to put the level data in before writing it away.
	TreeStorageNode node;
	char s[64];

	//The name of the level.
	if(!levelName.empty()){
		node.attributes["name"].push_back(levelName);

		//Update the level name in the levelpack.
		levels->getLevel()->name=levelName;
	}

	//The leveltheme.
	if(!levelTheme.empty())
		node.attributes["theme"].push_back(levelTheme);

	//target time and recordings.
	{
		char c[32];
		if(levelTime>=0){
			sprintf(c,"%d",levelTime);
			node.attributes["time"].push_back(c);

			//Update the target time the levelpack.
			levels->getLevel()->targetTime=levelTime;
		}
		if(levelRecordings>=0){
			sprintf(c,"%d",levelRecordings);
			node.attributes["recordings"].push_back(c);

			//Update the target recordings the levelpack.
			levels->getLevel()->targetRecordings=levelRecordings;
		}
	}

	//The width of the level.
	maxX=LEVEL_WIDTH;
	sprintf(s,"%d",maxX);
	node.attributes["size"].push_back(s);

	//The height of the level.
	maxY=LEVEL_HEIGHT;
	sprintf(s,"%d",maxY);
	node.attributes["size"].push_back(s);

	//Loop through the gameObjects and save them.
	for(int o=0;o<(signed)levelObjects.size();o++){
		int objectType=levelObjects[o]->type;

		//Check if it's a legal gameObject type.
		if(objectType>=0 && objectType<TYPE_MAX){
			TreeStorageNode* obj1=new TreeStorageNode;
			node.subNodes.push_back(obj1);

			//It's a tile so name the node tile.
			obj1->name="tile";

			//Write away the type of the gameObject.
			obj1->value.push_back(blockName[objectType]);

			//Get the box for the location of the gameObject.
			SDL_Rect box=levelObjects[o]->getBox(BoxType_Base);
			//Put the location and size in the storageNode.
			sprintf(s,"%d",box.x);
			obj1->value.push_back(s);
			sprintf(s,"%d",box.y);
			obj1->value.push_back(s);
			sprintf(s,"%d",box.w);
			obj1->value.push_back(s);
			sprintf(s,"%d",box.h);
			obj1->value.push_back(s);

			//Loop through the editor data and save it also.
			vector<pair<string,string> > obj;
			levelObjects[o]->getEditorData(obj);
			for(unsigned int i=0;i<obj.size();i++){
				if((!obj[i].first.empty()) && (!obj[i].second.empty())){
					obj1->attributes[obj[i].first].push_back(obj[i].second);
				}
			}

			//Loop through the scripts and add them to the storage node of the game object.
			map<int,string>::iterator it;
			Block* object=(dynamic_cast<Block*>(levelObjects[o]));
			for(it=object->scripts.begin();it!=object->scripts.end();++it){
				//Make sure the script isn't an empty string.
				if(it->second.empty())
					continue;
				
				TreeStorageNode* script=new TreeStorageNode;
				obj1->subNodes.push_back(script);

				script->name="script";
				script->value.push_back(gameObjectEventTypeMap[it->first]);

				script->attributes["script"].push_back(it->second);
			}
		}
	}

	//Loop through the level scripts and save them.
	map<int,string>::iterator it;
	for(it=scripts.begin();it!=scripts.end();++it){
		//Make sure the script isn't an empty string.
		if(it->second.empty())
			continue;

		TreeStorageNode* script=new TreeStorageNode;
		node.subNodes.push_back(script);

		script->name="script";
		script->value.push_back(levelEventTypeMap[it->first]);

		script->attributes["script"].push_back(it->second);
	}

	//Create a POASerializer and write away the level node.
	POASerializer objSerializer;
	objSerializer.writeNode(&node,save,true,true);
}


///////////////EVENT///////////////////
void LevelEditor::handleEvents(){
	//Check if we need to quit, if so we enter the exit state.
	if(event.type==SDL_QUIT){
		setNextState(STATE_EXIT);
	}

	//If playing/testing we should the game handle the events.
	if(playMode){
		Game::handleEvents();

		//Also check if we should exit the playMode.
		if(inputMgr.isKeyDownEvent(INPUTMGR_ESCAPE)){
			//Reset the game and disable playMode.
			Game::reset(true);
			playMode=false;
			GUIObjectRoot->visible=true;
			camera.x=cameraSave.x;
			camera.y=cameraSave.y;

			//NOTE: To prevent the mouse to still "be pressed" we set it to false.
			pressedLeftMouse=false;
		}
	}else{
		//Also check if we should exit the editor.
		if(inputMgr.isKeyDownEvent(INPUTMGR_ESCAPE)){
			//Before we quit ask a make sure question.
			if(msgBox(_("Are you sure you want to quit?"),MsgBoxYesNo,_("Quit prompt"))==MsgBoxYes){
				//We exit the level editor.
				setNextState(STATE_LEVEL_EDIT_SELECT);

				//Play the menu music again.
				getMusicManager()->playMusic("menu");

				//No need for handling other events, so return.
				return;
			}
		}

		//Check if we should redirect the event to the actions popup
		if(actionsPopup!=NULL){
			actionsPopup->handleEvents();
			return;
		}
		//Check if we should redirect the event to selection popup
		if(selectionPopup!=NULL){
			if(event.type==SDL_MOUSEBUTTONDOWN
				|| event.type==SDL_MOUSEBUTTONUP
				|| event.type==SDL_MOUSEMOTION)
			{
				selectionPopup->handleEvents();
				return;
			}
		}
		//TODO: Don't handle any Events when GUIWindows process them.
		{
			//Get the current mouse location.
			int x,y;
			SDL_GetMouseState(&x,&y);
			//Create the rectangle.
			SDL_Rect mouse={x,y,0,0};
			for(unsigned int i=0;i<GUIObjectRoot->childControls.size();i++){
				SDL_Rect box={0,0,0,0};
				box.x=GUIObjectRoot->childControls[i]->left;
				box.y=GUIObjectRoot->childControls[i]->top;
				box.w=GUIObjectRoot->childControls[i]->width;
				box.h=GUIObjectRoot->childControls[i]->height;
				if(checkCollision(mouse,box))
					return;
			}
		}

		//Check if toolbar is clicked.
		if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT && tooltip>=0){
			int t=tooltip;

			if(t<NUMBER_TOOLS){
				tool=(Tools)t;
			}else{
				//The selected button isn't a tool.
				//Now check which button it is.
				if(t==NUMBER_TOOLS){
					enterPlayMode();
				}
				if(t==NUMBER_TOOLS+2){
					//Open up level settings dialog
					levelSettings();
				}
				if(t==NUMBER_TOOLS+4){
					//Go back to the level selection screen of Level Editor
					setNextState(STATE_LEVEL_EDIT_SELECT);
					//Change the music back to menu music.
					getMusicManager()->playMusic("menu");
				}
				if(t==NUMBER_TOOLS+3){
					//Save current level
					saveLevel(levelFile);
					//And give feedback to the user.
					if(levelName.empty())
						msgBox(tfm::format(_("Level \"%s\" saved"),fileNameFromPath(levelFile)),MsgBoxOKOnly,_("Saved"));
					else
						msgBox(tfm::format(_("Level \"%s\" saved"),levelName),MsgBoxOKOnly,_("Saved"));
				}
			}

			return;
		}

		//Check if tool box is clicked.
		if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT && toolboxRect.w>0){
			if(toolboxVisible){
				if(event.button.y<64){
					//Check if we need to hide it
					if(event.button.x>=SCREEN_WIDTH-24 && event.button.x<SCREEN_WIDTH && event.button.y<20){
						toolboxVisible=false;
						return;
					}

					//Check if a block is clicked.
					if(event.button.x>=24 && event.button.x<SCREEN_WIDTH-24){
						int m=(SCREEN_WIDTH-48)/64;
						int i=(event.button.x-24)/64;
						if(i<m && i+toolboxIndex<EDITOR_ORDER_MAX){
							currentType=i+toolboxIndex;
						}
					}

					//TODO: Move left and move right button.

					return;
				}
			}else if(event.button.x>=toolboxRect.x && event.button.x<toolboxRect.x+toolboxRect.w
				&& event.button.y>=toolboxRect.y && event.button.y<toolboxRect.y+toolboxRect.h)
			{
				toolboxVisible=true;
				return;
			}
		}

		//Check if shift is pressed.
		pressedShift=inputMgr.isKeyDown(INPUTMGR_SHIFT);

		//Check if delete is pressed.
		if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_DELETE){
			if(!selection.empty()){
				//Loop through the selected game objects.
				 while(!selection.empty()){
					//Remove the objects in the selection.
					removeObject(selection[0]);
				}

				//And clear the selection vector.
				selection.clear();
				dragCenter=NULL;
				selectionDrag=false;
			}
		}

		//Check for copy (Ctrl+c) or cut (Ctrl+x).
		if(event.type==SDL_KEYDOWN && (event.key.keysym.sym==SDLK_c || event.key.keysym.sym==SDLK_x) && (event.key.keysym.mod & KMOD_CTRL)){
			//Clear the current clipboard.
			clipboard.clear();

			//Check if the selection isn't empty.
			if(!selection.empty()){
				//Loop through the selection to find the left-top block.
				int x=selection[0]->getBox().x;
				int y=selection[0]->getBox().y;
				for(unsigned int o=1; o<selection.size(); o++){
					if(selection[o]->getBox().x<x || selection[o]->getBox().y<y){
						x=selection[o]->getBox().x;
						y=selection[o]->getBox().y;
					}
				}

				//Loop through the selection for the actual copying.
				for(unsigned int o=0; o<selection.size(); o++){
					//Get the editor data of the object.
					vector<pair<string,string> > obj;
					selection[o]->getEditorData(obj);

					//Loop through the editor data and convert it.
					map<string,string> objMap;
					for(unsigned int i=0;i<obj.size();i++){
						objMap[obj[i].first]=obj[i].second;
					}
					//Add some entries to the map.
					char s[64];
					sprintf(s,"%d",selection[o]->getBox().x-x);
					objMap["x"]=s;
					sprintf(s,"%d",selection[o]->getBox().y-y);
					objMap["y"]=s;
					sprintf(s,"%d",selection[o]->type);
					objMap["type"]=s;

					//Overwrite the id to prevent triggers, portals, buttons, movingblocks, etc. from malfunctioning.
					//We give an empty string as id, which is invalid and thus suitable.
					objMap["id"]="";
					//Do the same for destination if the type is portal.
					if(selection[o]->type==TYPE_PORTAL){
						objMap["destination"]="";
					}

					//And add the map to the clipboard vector.
					clipboard.push_back(objMap);

					if(event.key.keysym.sym==SDLK_x){
						//Cutting means deleting the game object.
						removeObject(selection[o]);
						o--;
					}
				}

				//Only clear the selection when Ctrl+x;
				if(event.key.keysym.sym==SDLK_x){
					selection.clear();
					dragCenter=NULL;
					selectionDrag=false;
				}
			}
		}

		//Check for paste (Ctrl+v).
		if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_v && (event.key.keysym.mod & KMOD_CTRL)){
			//First make sure that the clipboard isn't empty.
			if(!clipboard.empty()){
				//Clear the current selection.
				selection.clear();

				//Get the current mouse location.
				int x,y;
				SDL_GetMouseState(&x,&y);
				x+=camera.x;
				y+=camera.y;

				//Apply snap to grid.
				if(!pressedShift){
					snapToGrid(&x,&y);
				}else{
					x-=25;
					y-=25;
				}

				//Integers containing the diff of the x that occurs when placing a block outside the level size on the top or left.
				//We use it to compensate the corrupted x and y locations of the other clipboard blocks.
				int diffX=0;
				int diffY=0;


				//Loop through the clipboard.
				for(unsigned int o=0;o<clipboard.size();o++){
					Block* block=new Block(this,0,0,50,50,atoi(clipboard[o]["type"].c_str()));
					block->setBaseLocation(atoi(clipboard[o]["x"].c_str())+x+diffX,atoi(clipboard[o]["y"].c_str())+y+diffY);
					block->setEditorData(clipboard[o]);

					if(block->getBox().x<0){
						//A block on the left side of the level, meaning we need to shift everything.
						//First calc the difference.
						diffX+=(0-(block->getBox().x));
					}
					if(block->getBox().y<0){
						//A block on the left side of the level, meaning we need to shift everything.
						//First calc the difference.
						diffY+=(0-(block->getBox().y));
					}

					//And add the object using the addObject method.
					addObject(block);

					//Also add the block to the selection.
					selection.push_back(block);
				}
			}
		}

		//Check if the return button is pressed.
		//If so run the configure tool.
		if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_RETURN){
			//Get the current mouse location.
			int x,y;
			SDL_GetMouseState(&x,&y);
			//Create the rectangle.
			SDL_Rect mouse={x+camera.x,y+camera.y,0,0};

			//Loop through the selected game objects.
			for(unsigned int o=0; o<levelObjects.size(); o++){
				//Check for collision.
				if(checkCollision(mouse,levelObjects[o]->getBox())){
					//Invoke the onEnterObject.
					onEnterObject(levelObjects[o]);
					//Break out of the for loop.
					break;
				}
			}
		}

		//Check for the arrow keys, used for moving the camera when playMode=false.
		cameraXvel=0;
		cameraYvel=0;
		if(inputMgr.isKeyDown(INPUTMGR_RIGHT)){
			if(pressedShift){
				cameraXvel+=10;
			}else{
				cameraXvel+=5;
			}
		}
		if(inputMgr.isKeyDown(INPUTMGR_LEFT)){
			if(pressedShift){
				cameraXvel-=10;
			}else{
				cameraXvel-=5;
			}
		}
		if(inputMgr.isKeyDown(INPUTMGR_UP)){
			if(pressedShift){
				cameraYvel-=10;
			}else{
				cameraYvel-=5;
			}
		}
		if(inputMgr.isKeyDown(INPUTMGR_DOWN)){
			if(pressedShift){
				cameraYvel+=10;
			}else{
				cameraYvel+=5;
			}
		}

		//Check if the left mouse button is pressed/holded.
		if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT){
			pressedLeftMouse=true;
		}
		if(event.type==SDL_MOUSEBUTTONUP && event.button.button==SDL_BUTTON_LEFT){
			pressedLeftMouse=false;

			//We also need to check if dragging is true.
			if(dragging){
				//Set dragging false and call the onDrop event.
				dragging=false;
				int x,y;
				SDL_GetMouseState(&x,&y);
				//We call the drop event.
				onDrop(x+camera.x,y+camera.y);
			}
		}

		//Check if the mouse is dragging.
		if(pressedLeftMouse && event.type==SDL_MOUSEMOTION){
			if(abs(event.motion.xrel)+abs(event.motion.yrel)>=2){
				//Check if this is the start of the dragging.
				if(!dragging){
					//The mouse is moved enough so let's set dragging true.
					dragging=true;
					//Get the current mouse location.
					int x,y;
					SDL_GetMouseState(&x,&y);
					//We call the dragStart event.
					onDragStart(x+camera.x,y+camera.y);
				}else{
					//Dragging was already true meaning we call onDrag() instead of onDragStart().
					onDrag(event.motion.xrel,event.motion.yrel);
				}
			}
		}
		
		//Update cursor.
		if(dragging){
			if(tool==REMOVE)
				currentCursor=CURSOR_REMOVE;
			else
				currentCursor=CURSOR_DRAG;
		}

		//Get the current mouse location.
		int x,y;
		SDL_GetMouseState(&x,&y);
		//Create the rectangle.
		SDL_Rect mouse={x,y,0,0};

		//Check if we scroll up, meaning the currentType++;
		if((event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_WHEELUP) || inputMgr.isKeyDownEvent(INPUTMGR_NEXT)){
			switch(tool){
			case ADD:
				//Check if mouse is in tool box.
				if(toolboxVisible && toolboxRect.w>0){
					int x,y;
					SDL_GetMouseState(&x,&y);
					if(y<64){
						toolboxIndex-=2;
						if(toolboxIndex<0) toolboxIndex=0;
						break;
					}
				}
				//Only change the current type when using the add tool.
				currentType++;
				if(currentType>=EDITOR_ORDER_MAX){
					currentType=0;
				}
				break;
			case SELECT:
				//When configuring moving blocks.
				if(moving){
					movingSpeed++;
					//The movingspeed is capped at 100.
					if(movingSpeed>100){
						movingSpeed=100;
					}
					break;
				}
				//Fall through.
			default:
				//When in other mode, just scrolling the map
				if(pressedShift)
					camera.x-=200;
				else camera.y-=200;
				break;
			}
		}
		//Check if we scroll down, meaning the currentType--;
		if((event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_WHEELDOWN) || inputMgr.isKeyDownEvent(INPUTMGR_PREVIOUS)){
			switch(tool){
			case ADD:
				//Check if mouse is in tool box.
				if(toolboxVisible && toolboxRect.w>0){
					int x,y;
					SDL_GetMouseState(&x,&y);
					if(y<64){
						int m=EDITOR_ORDER_MAX-(SCREEN_WIDTH-48)/64;
						toolboxIndex+=2;
						if(toolboxIndex>m) toolboxIndex=m;
						if(toolboxIndex<0) toolboxIndex=0;
						break;
					}
				}
				//Only change the current type when using the add tool.
				currentType--;
				if(currentType<0){
					currentType=EDITOR_ORDER_MAX-1;
				}
				break;
			case SELECT:
				//When configuring moving blocks.
				if(moving){
					movingSpeed--;
					if(movingSpeed<=0){
						movingSpeed=1;
					}
					break;
				}
				//Fall through.
			default:
				//When in other mode, just scrolling the map
				if(pressedShift) camera.x+=200;
				else camera.y+=200;
				break;
			}
		}

		//Check if we should enter playMode.
		if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_p){
			enterPlayMode();
		}
		//Check for tool shortcuts.
		if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_a){
			tool=ADD;
		}
		if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_s){
			tool=SELECT;
		}
		if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_d){
			//We clear the selection since that can't be used in the deletion tool.
			selection.clear();
			tool=REMOVE;
		}

		//Check for certain events.

		//First make sure the mouse isn't above the toolbar.
		if(!checkCollision(mouse,toolbarRect) && !checkCollision(mouse,toolboxRect)){
			mouse.x+=camera.x;
			mouse.y+=camera.y;

			//Boolean if there's a click event fired.
			bool clickEvent=false;
			//Check if a mouse button is pressed.
			if(event.type==SDL_MOUSEBUTTONDOWN){
				std::vector<GameObject*> clickObjects;

				//Loop through the objects to check collision.
				for(unsigned int o=0; o<levelObjects.size(); o++){
					if(checkCollision(levelObjects[o]->getBox(),mouse)==true){
						clickObjects.push_back(levelObjects[o]);
					}
				}

				//Check if there are multiple objects above eachother or just one.
				if(clickObjects.size()==1){
					//We have collision meaning that the mouse is above an object.
					std::vector<GameObject*>::iterator it;
					it=find(selection.begin(),selection.end(),clickObjects[0]);

					//Set event true since there's a click event.
					clickEvent=true;

					//Check if the clicked object is in the selection or not.
					bool isSelected=(it!=selection.end());
					if(event.button.button==SDL_BUTTON_LEFT){
						onClickObject(clickObjects[0],isSelected);
					}else if(event.button.button==SDL_BUTTON_RIGHT){
						onRightClickObject(clickObjects[0],isSelected);
					}
				}else if(clickObjects.size()>1){
					//There are more than one object under the mouse
					std::vector<GameObject*>::iterator it;
					it=find(selection.begin(),selection.end(),clickObjects[0]);

					//Set event true since there's a click event.
					clickEvent=true;

					//Check if the clicked object is in the selection or not.
					bool isSelected=(it!=selection.end());

					//Only show the selection popup when right clicking.
					if(event.button.button==SDL_BUTTON_LEFT){
						onClickObject(clickObjects[0],isSelected);
					}else if(event.button.button==SDL_BUTTON_RIGHT){
						//Remove the selection popup if there's one.
						if(selectionPopup!=NULL)
							delete selectionPopup;

						//Get the mouse location.
						int x,y;
						SDL_GetMouseState(&x,&y);
						selectionPopup=new LevelEditorSelectionPopup(this,clickObjects,x,y);
					}
				}
			}

			//If event is false then we clicked on void.
			if(!clickEvent){
				if(event.type==SDL_MOUSEBUTTONDOWN){
					if(event.button.button==SDL_BUTTON_LEFT){
						//Left mouse button on void.
						onClickVoid(mouse.x,mouse.y);
					}else if(event.button.button==SDL_BUTTON_RIGHT /*&& tool==SELECT*/){
						//Stop linking.
						if(linking){
							linking=false;
							linkingTrigger=NULL;
							//NOTE: We shouldn't be able to be linking AND moving so return to prevent actions popup.
							return;
						}

						//Write the path to the moving block.
						if(moving){
							std::map<std::string,std::string> editorData;
							char s[64], s0[64];

							sprintf(s,"%d",int(movingBlocks[movingBlock].size()));
							editorData["MovingPosCount"]=s;
							//Loop through the positions.
							for(unsigned int o=0;o<movingBlocks[movingBlock].size();o++){
								sprintf(s0+1,"%u",o);
								sprintf(s,"%d",movingBlocks[movingBlock][o].x);
								s0[0]='x';
								editorData[s0]=s;
								sprintf(s,"%d",movingBlocks[movingBlock][o].y);
								s0[0]='y';
								editorData[s0]=s;
								sprintf(s,"%d",movingBlocks[movingBlock][o].time);
								s0[0]='t';
								editorData[s0]=s;
							}
							movingBlock->setEditorData(editorData);

							//Stop moving.
							moving=false;
							movingBlock=NULL;
							return;
						}

						//No return so far so call onRightClickVoid.
						onRightClickVoid(mouse.x,mouse.y);
					}
				}
			}
		}

		//Check for backspace when moving to remove a movingposition.
		if(moving && event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_BACKSPACE){
			if(movingBlocks[movingBlock].size()>0){
				movingBlocks[movingBlock].pop_back();
			}
		}

		//Check for the tab key, level settings.
		if(inputMgr.isKeyDownEvent(INPUTMGR_TAB)){
			//Show the levelSettings.
			levelSettings();
		}

		//NOTE: Do we even need Ctrl+n?
		//Check if we should a new level. (Ctrl+n)
		//if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_n && (event.key.keysym.mod & KMOD_CTRL)){
		//	reset();
		//	//NOTE: We don't have anything to load from so we create an empty TreeStorageNode.
		//	Game::loadLevelFromNode(new TreeStorageNode,"");

		//	//Hide selection popup (if any)
		//	if(selectionPopup!=NULL){
		//		delete selectionPopup;
		//		selectionPopup=NULL;
		//	}
		//}
		//Check if we should save the level (Ctrl+s).
		if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_s && (event.key.keysym.mod & KMOD_CTRL)){
			saveLevel(levelFile);
			//And give feedback to the user.
			if(levelName.empty())
				msgBox(tfm::format(_("Level \"%s\" saved"),fileNameFromPath(levelFile)),MsgBoxOKOnly,_("Saved"));
			else
				msgBox(tfm::format(_("Level \"%s\" saved"),levelName),MsgBoxOKOnly,_("Saved"));
		}
	}
}

void LevelEditor::enterPlayMode(){
	//Check if we are already in play mode.
	if(playMode) return;

	//Stop linking or moving.
	if(linking){
		linking=false;
		linkingTrigger=NULL;
	}

	if(moving){
		//Write the path to the moving block.
		std::map<std::string,std::string> editorData;
		char s[64], s0[64];

		sprintf(s,"%d",int(movingBlocks[movingBlock].size()));
		editorData["MovingPosCount"]=s;
		//Loop through the positions.
		for(unsigned int o=0;o<movingBlocks[movingBlock].size();o++){
			sprintf(s0+1,"%u",o);
			sprintf(s,"%d",movingBlocks[movingBlock][o].x);
			s0[0]='x';
			editorData[s0]=s;
			sprintf(s,"%d",movingBlocks[movingBlock][o].y);
			s0[0]='y';
			editorData[s0]=s;
			sprintf(s,"%d",movingBlocks[movingBlock][o].time);
			s0[0]='t';
			editorData[s0]=s;
		}
		movingBlock->setEditorData(editorData);

		moving=false;
		movingBlock=NULL;
	}

	//Change mode.
	playMode=true;
	GUIObjectRoot->visible=false;
	cameraSave.x=camera.x;
	cameraSave.y=camera.y;

	//Compile and run script.
	//NOTE: The scriptExecutor should have been reset because we called Game::reset() before.
	compileScript();
}

void LevelEditor::levelSettings(){
	//It isn't so open a popup asking for a name.
	GUIWindow* root=new GUIWindow((SCREEN_WIDTH-600)/2,(SCREEN_HEIGHT-300)/2,600,300,true,true,_("Level settings"));
	root->name="lvlSettingsWindow";
	root->eventCallback=this;
	GUIObject* obj;

	//Create the two textboxes with a label.
	obj=new GUILabel(40,50,240,36,_("Name:"));
	root->addChild(obj);
	obj=new GUITextBox(140,50,410,36,levelName.c_str());
	obj->name="name";
	root->addChild(obj);

	obj=new GUILabel(40,100,240,36,_("Theme:"));
	root->addChild(obj);
	obj=new GUITextBox(140,100,410,36,levelTheme.c_str());
	obj->name="theme";
	root->addChild(obj);

	//target time and recordings.
	{
		obj=new GUILabel(40,150,240,36,_("Target time (s):"));
		root->addChild(obj);
		GUISpinBox* obj2=new GUISpinBox(290,150,260,36);
		obj2->name="time";
		
		ostringstream ss;
		ss << levelTime/40.0f;
		obj2->caption=ss.str();
		obj2->update();
		
		obj2->limitMin=0.0f;
		obj2->change=0.1f;
		root->addChild(obj2);

		obj=new GUILabel(40,200,240,36,_("Target recordings:"));
		root->addChild(obj);
		obj2=new GUISpinBox(290,200,260,36);
		
		ostringstream ss2;
		ss2 << levelRecordings;
		obj2->caption=ss2.str();
		
		obj2->limitMin=0.0f;
		obj2->format="%1.0f";
		obj2->name="recordings";
		obj2->update();
		root->addChild(obj2);
	}


	//Ok and cancel buttons.
	obj=new GUIButton(root->width*0.3,300-44,-1,36,_("OK"),0,true,true,GUIGravityCenter);
	obj->name="lvlSettingsOK";
	obj->eventCallback=root;
	root->addChild(obj);
	obj=new GUIButton(root->width*0.7,300-44,-1,36,_("Cancel"),0,true,true,GUIGravityCenter);
	obj->name="lvlSettingsCancel";
	obj->eventCallback=root;
	root->addChild(obj);

	GUIObjectRoot->addChild(root);
}

void LevelEditor::postLoad(){
	//We need to find the triggers.
	for(unsigned int o=0;o<levelObjects.size();o++){
		//Check for the highest id.
		unsigned int id=atoi(levelObjects[o]->getEditorProperty("id").c_str());
		if(id>=currentId)
			currentId=id+1;

		switch(levelObjects[o]->type){
			case TYPE_BUTTON:
			case TYPE_SWITCH:
			{
				//Add the object to the triggers vector.
				vector<GameObject*> linked;
				triggers[levelObjects[o]]=linked;
				//Now loop through the levelObjects in search for objects with the same id.
				for(unsigned int oo=0;oo<levelObjects.size();oo++){
					//Check if it isn't the same object but has the same id.
					if(o!=oo && (dynamic_cast<Block*>(levelObjects[o]))->id==(dynamic_cast<Block*>(levelObjects[oo]))->id){
						//Add the object to the link vector of the trigger.
						triggers[levelObjects[o]].push_back(levelObjects[oo]);
					}
				}
				break;
			}
			case TYPE_PORTAL:
			{
				//Add the object to the triggers vector.
				vector<GameObject*> linked;
				triggers[levelObjects[o]]=linked;

				//If the destination is empty we return.
				if((dynamic_cast<Block*>(levelObjects[o]))->destination.empty()){
					break;
				}

				//Now loop through the levelObjects in search for objects with the same id as destination.
				for(unsigned int oo=0;oo<levelObjects.size();oo++){
					//Check if it isn't the same object but has the same id.
					if(o!=oo && (dynamic_cast<Block*>(levelObjects[o]))->destination==(dynamic_cast<Block*>(levelObjects[oo]))->id){
						//Add the object to the link vector of the trigger.
						triggers[levelObjects[o]].push_back(levelObjects[oo]);
					}
				}
				break;
			}
			case TYPE_MOVING_BLOCK:
			case TYPE_MOVING_SHADOW_BLOCK:
			case TYPE_MOVING_SPIKES:
			{
				//Get the editor data.
				vector<pair<string,string> > objMap;
				levelObjects[o]->getEditorData(objMap);
				
				//Add the object to the movingBlocks vector.
				vector<MovingPosition> positions;
				movingBlocks[levelObjects[o]]=positions;

				//Get the number of entries of the editor data.
				int m=objMap.size();

				//Check if the editor data isn't empty.
				if(m>0){
					//Integer containing the positions.
					int pos;
					int currentPos=0;

					//Get the number of movingpositions.
					pos=atoi(objMap[1].second.c_str());

					while(currentPos<pos){
						int x=atoi(objMap[currentPos*3+4].second.c_str());
						int y=atoi(objMap[currentPos*3+5].second.c_str());
						int t=atoi(objMap[currentPos*3+6].second.c_str());

						//Create a new movingPosition.
						MovingPosition position(x,y,t);
						movingBlocks[levelObjects[o]].push_back(position);

						//Increase currentPos by one.
						currentPos++;
					}
				}
				break;
			}
			default:
				break;
		}
	}
}

void LevelEditor::snapToGrid(int* x,int* y){
	//Check if the x location is negative.
	if(*x<0){
		*x=-((abs(*x-50)/50)*50);
	}else{
		*x=(*x/50)*50;
	}

	//Now the y location.
	if(*y<0){
		*y=-((abs(*y-50)/50)*50);
	}else{
		*y=(*y/50)*50;
	}
}

void LevelEditor::setCamera(const SDL_Rect* r,int count){	
	//SetCamera only works in the Level editor and when mouse is inside window.
	if(stateID==STATE_LEVEL_EDITOR&&(SDL_GetAppState()&SDL_APPMOUSEFOCUS)){
		//Get the mouse coordinates.
		int x,y;
		SDL_GetMouseState(&x,&y);
		SDL_Rect mouse={x,y,0,0};
		
		//Don't continue here if mouse is inside one of the boxes given as parameter.
		for(int i=0;i<count;i++){
			if(checkCollision(mouse,r[i]))
				return;
		}

		//Check if the mouse is near the left edge of the screen.
		//Else check if the mouse is near the right edge.
		if(x<50){
			//We're near the left edge so move the camera.
			camera.x-=5;
		}else if(x>SCREEN_WIDTH-50){
			//We're near the right edge so move the camera.
			camera.x+=5;
		}

		//Check if the tool box is visible and we need to calc screen size correctly.
		int y0=50;
		if(toolboxVisible && toolboxRect.w>0) y0+=64;

		//Check if the mouse is near the top edge of the screen.
		//Else check if the mouse is near the bottom edge.
		if(y<y0){
			//We're near the top edge so move the camera.
			camera.y-=5;
		}else if(y>SCREEN_HEIGHT-50){
			//We're near the bottom edge so move the camera.
			camera.y+=5;
		}
	}
}

void LevelEditor::onClickObject(GameObject* obj,bool selected){
	switch(tool){
		case SELECT:
		{
			//Check if we are linking.
			if(linking){
				//Check if the obj is valid to link to.
				switch(obj->type){
					case TYPE_CONVEYOR_BELT:
					case TYPE_SHADOW_CONVEYOR_BELT:
					case TYPE_MOVING_BLOCK:
					case TYPE_MOVING_SHADOW_BLOCK:
					case TYPE_MOVING_SPIKES:
					{
						//It's only valid when not linking a portal.
						if(linkingTrigger->type==TYPE_PORTAL){
							//You can't link a portal to moving blocks, etc.
							//Stop linking and return.
							linkingTrigger=NULL;
							linking=false;
							return;
						}
						break;
					}
					case TYPE_PORTAL:
					{
						//Make sure that the linkingTrigger is also a portal.
						if(linkingTrigger->type!=TYPE_PORTAL){
							//The linkingTrigger isn't a portal so stop linking and return.
							linkingTrigger=NULL;
							linking=false;
							return;
						}
						break;
					}
					default:
						//It isn't valid so stop linking and return.
						linkingTrigger=NULL;
						linking=false;
						return;
					break;
				}

				//Check if the linkingTrigger can handle multiple or only one link.
				switch(linkingTrigger->type){
					case TYPE_PORTAL:
					{
						//Portals can only link to one so remove all existing links.
						triggers[linkingTrigger].clear();
						triggers[linkingTrigger].push_back(obj);
						break;
					}
					default:
					{
						//The most can handle multiple links.
						triggers[linkingTrigger].push_back(obj);
						break;
					}
				}

				//Check if it's a portal.
				if(linkingTrigger->type==TYPE_PORTAL){
					//Portals need to get the id of the other instead of give it's own id.
					char s[64];
					sprintf(s,"%d",atoi(obj->getEditorProperty("id").c_str()));
					linkingTrigger->setEditorProperty("destination",s);
				}else{
					//Give the object the same id as the trigger.
					char s[64];
					sprintf(s,"%d",atoi(linkingTrigger->getEditorProperty("id").c_str()));
					obj->setEditorProperty("id",s);
				}

				//We return to prevent configuring stuff like conveyor belts, etc...
				linking=false;
				linkingTrigger=NULL;
				return;
			}

			//If we're moving add a movingposition.
			if(moving){
				//Get the current mouse location.
				int x,y;
				SDL_GetMouseState(&x,&y);
				x+=camera.x;
				y+=camera.y;

				//Apply snap to grid.
				if(!pressedShift){
					snapToGrid(&x,&y);
				}else{
					x-=25;
					y-=25;
				}

				x-=movingBlock->getBox().x;
				y-=movingBlock->getBox().y;

				//Calculate the length.
				//First get the delta x and y.
				int dx,dy;
				if(movingBlocks[movingBlock].empty()){
					dx=x;
					dy=y;
				}else{
					dx=x-movingBlocks[movingBlock].back().x;
					dy=y-movingBlocks[movingBlock].back().y;
				}

				double length=sqrt(double(dx*dx+dy*dy));
				movingBlocks[movingBlock].push_back(MovingPosition(x,y,(int)(length*(10/(double)movingSpeed))));
				return;
			}
		}
		case ADD:
		{
			//Check if object is already selected.
			if(!selected){
				//First check if shift is pressed or not.
				if(!pressedShift){
					//Clear the selection.
					selection.clear();
				}

				//Add the object to the selection.
				selection.push_back(obj);
			}
			break;
		}
		case REMOVE:
		{
			//Remove the object.
			removeObject(obj);
			break;
		}
		default:
			break;
	}
}

void LevelEditor::onRightClickObject(GameObject* obj,bool selected){
	//Create an actions popup for the game object.
	if(actionsPopup==NULL){
		//Get the mouse location.
		int x,y;
		SDL_GetMouseState(&x,&y);
		actionsPopup=new LevelEditorActionsPopup(this,obj,x,y);
		return;
	}
}

void LevelEditor::onClickVoid(int x,int y){
	switch(tool){
		case ADD:
		{
			//We need to clear the selection.
			selection.clear();
	
			//Now place an object.
			//Apply snap to grid.
			if(!pressedShift){
				snapToGrid(&x,&y);
			}else{
				x-=25;
				y-=25;
			}
			addObject(new Block(this,x,y,50,50,editorTileOrder[currentType]));
			break;
		}
		case SELECT:
		{
			//We need to clear the selection.
			selection.clear();

			//If we're linking we should stop, user abort.
			if(linking){
				linking=false;
				linkingTrigger=NULL;
				//And return.
				return;
			}

			//If we're moving we should add a point.
			if(moving){
				//Apply snap to grid.
				if(!pressedShift){
					snapToGrid(&x,&y);
				}else{
					x-=25;
					y-=25;
				}

				x-=movingBlock->getBox().x;
				y-=movingBlock->getBox().y;

				//Calculate the length.
				//First get the delta x and y.
				int dx,dy;
				if(movingBlocks[movingBlock].empty()){
					dx=x;
					dy=y;
				}else{
					dx=x-movingBlocks[movingBlock].back().x;
					dy=y-movingBlocks[movingBlock].back().y;
				}

				double length=sqrt(double(dx*dx+dy*dy));
				movingBlocks[movingBlock].push_back(MovingPosition(x,y,(int)(length*(10/(double)movingSpeed))));

				//And return.
				return;
			}
			break;
		}
		default:
			break;
	}
}

void LevelEditor::onRightClickVoid(int x,int y){
	//Create an actions popup for the game object.
	if(actionsPopup==NULL){
		//Get the mouse location.
		int x,y;
		SDL_GetMouseState(&x,&y);
		actionsPopup=new LevelEditorActionsPopup(this,NULL,x,y);
		return;
	}
}

void LevelEditor::onDragStart(int x,int y){
	switch(tool){
		case SELECT:
		case ADD:
		{
			//We can drag the selection so check if the selection isn't empty.
			if(!selection.empty()){
				//The selection isn't empty so search the dragCenter.
				//Create a mouse rectangle.
				SDL_Rect mouse={x,y,0,0};
			
				//Loop through the objects to check collision.
				for(unsigned int o=0; o<selection.size(); o++){
					if(checkCollision(selection[o]->getBox(),mouse)==true){
						//We have collision so set the dragCenter.
						dragCenter=selection[o];
						selectionDrag=true;
					}
				}
			}
			break;
		}
		default:
			break;
	}
}

void LevelEditor::onDrag(int dx,int dy){
	switch(tool){
	  case REMOVE:
	  {
		//No matter what we delete the item the mouse is above.
		//Get the current mouse location.
		int x,y;
		SDL_GetMouseState(&x,&y);
		//Create the rectangle.
		SDL_Rect mouse={x+camera.x,y+camera.y,0,0};
		
		currentCursor=CURSOR_REMOVE;

		//Loop through the objects to check collision.
		for(unsigned int o=0; o<levelObjects.size(); o++){
			if(checkCollision(levelObjects[o]->getBox(),mouse)==true){
				//Remove the object.
				removeObject(levelObjects[o]);
			}
		}
	    break;
	  }
	  default:
	    break;
	}
}

void LevelEditor::onDrop(int x,int y){
	switch(tool){
		case SELECT:
		case ADD:
		{
			//Check if the drag center isn't null.
			if(dragCenter==NULL)
				return;
			//The location of the dragCenter.
			SDL_Rect r=dragCenter->getBox();
			//Apply snap to grid.
			if(!pressedShift){
				snapToGrid(&x,&y);
			}else{
				x-=25;
				y-=25;
			}

			//Loop through the selection.
			for(unsigned int o=0; o<selection.size(); o++){
				SDL_Rect r1=selection[o]->getBox();
				//We need to place the object at his drop place.
				moveObject(selection[o],(r1.x-r.x)+x,(r1.y-r.y)+y);
			}

			//Make sure the dragCenter is null and set selectionDrag false.
			dragCenter=NULL;
			selectionDrag=false;
			break;
		}
		default:
			break;
	}
}

void LevelEditor::onCameraMove(int dx,int dy){
	switch(tool){
		case REMOVE:
		{
			//Only delete when the left mouse button is pressed.
			if(pressedLeftMouse){
				//Get the current mouse location.
				int x,y;
				SDL_GetMouseState(&x,&y);
				//Create the rectangle.
				SDL_Rect mouse={x+camera.x,y+camera.y,0,0};

				//Loop through the objects to check collision.
				for(unsigned int o=0; o<levelObjects.size(); o++){
					if(checkCollision(levelObjects[o]->getBox(),mouse)==true){
						//Remove the object.
						removeObject(levelObjects[o]);
					}
				}
			}
			break;
		}
		default:
			break;
	}
}

void LevelEditor::onEnterObject(GameObject* obj){
	//NOTE: Function isn't used anymore.
}

void LevelEditor::addObject(GameObject* obj){
	//Increase totalCollectables everytime we add a new collectable
	if(obj->type==TYPE_COLLECTABLE) {
		totalCollectables++;
	}

	//If it's a player or shadow start then we need to remove the previous one.
	if(obj->type==TYPE_START_PLAYER || obj->type==TYPE_START_SHADOW){
		//Loop through the levelObjects.
		for(unsigned int o=0; o<levelObjects.size(); o++){
			//Check if the type is the same.
			if(levelObjects[o]->type==obj->type){
				removeObject(levelObjects[o]);
			}
		}
	}
	//Add it to the levelObjects.
	Block* block=dynamic_cast<Block*>(obj);
	//Make sure it's a block.
	if(!block)
		return;
	levelObjects.push_back(block);

	//Check if the object is inside the level dimensions, etc.
	//Just call moveObject() to perform this.
	moveObject(obj,obj->getBox().x,obj->getBox().y);

	//GameObject type specific stuff.
	switch(obj->type){
		case TYPE_BUTTON:
		case TYPE_SWITCH:
		case TYPE_PORTAL:
		{
			//Add the object to the triggers.
			vector<GameObject*> linked;
			triggers[block]=linked;

			//Give it it's own id.
			char s[64];
			sprintf(s,"%u",currentId);
			currentId++;
			block->setEditorProperty("id",s);
			break;
		}
		case TYPE_MOVING_BLOCK:
		case TYPE_MOVING_SHADOW_BLOCK:
		case TYPE_MOVING_SPIKES:
		{
			//Add the object to the moving blocks.
			vector<MovingPosition> positions;
			movingBlocks[block]=positions;

			//Get the editor data.
			vector<pair<string,string> > objMap;
			block->getEditorData(objMap);

			//Get the number of entries of the editor data.
			int m=objMap.size();

			//Check if the editor data isn't empty.
			if(m>0){
				//Integer containing the positions.
				int pos=0;
				int currentPos=0;

				//Get the number of movingpositions.
				pos=atoi(objMap[1].second.c_str());

				while(currentPos<pos){
					int x=atoi(objMap[currentPos*3+4].second.c_str());
					int y=atoi(objMap[currentPos*3+5].second.c_str());
					int t=atoi(objMap[currentPos*3+6].second.c_str());

					//Create a new movingPosition.
					MovingPosition position(x,y,t);
					movingBlocks[block].push_back(position);

					//Increase currentPos by one.
					currentPos++;
				}
			}

			//Give it it's own id.
			std::map<std::string,std::string> editorData;
			char s[64];
			sprintf(s,"%u",currentId);
			currentId++;
			editorData["id"]=s;
			block->setEditorData(editorData);
			break;
		}
		default:
			break;
	}
}

void LevelEditor::moveObject(GameObject* obj,int x,int y){
	//Set the obj at it's new position.
	obj->setBaseLocation(x,y);

	//Check if the object is inside the level dimensions.
	//If not let the level grow.
	if(obj->getBox().x+50>LEVEL_WIDTH){
		LEVEL_WIDTH=obj->getBox().x+50;
	}
	if(obj->getBox().y+50>LEVEL_HEIGHT){
		LEVEL_HEIGHT=obj->getBox().y+50;
	}
	if(obj->getBox().x<0 || obj->getBox().y<0){
		//A block on the left (or top) side of the level, meaning we need to shift everything.
		//First calc the difference.
		int diffx=(0-(obj->getBox().x));
		int diffy=(0-(obj->getBox().y));

		if(diffx<0) diffx=0;
		if(diffy<0) diffy=0;

		//Change the level size first.
		//The level grows with the difference, 0-(x+50).
		LEVEL_WIDTH+=diffx;
		LEVEL_HEIGHT+=diffy;
		//cout<<"x:"<<diffx<<",y:"<<diffy<<endl; //debug
		camera.x+=diffx;
		camera.y+=diffy;

		//Set the position of player and shadow
		//(although it's unnecessary if there is player and shadow start)
		player.setLocation(player.getBox().x+diffx,player.getBox().y+diffy);
		shadow.setLocation(shadow.getBox().x+diffx,shadow.getBox().y+diffy);

		for(unsigned int o=0; o<levelObjects.size(); o++){
			//FIXME: shouldn't recuesive call me (to prevent stack overflow bugs)
			moveObject(levelObjects[o],levelObjects[o]->getBox().x+diffx,levelObjects[o]->getBox().y+diffy);
		}
	}

	//If the object is a player or shadow start then change the start position of the player or shadow.
	if(obj->type==TYPE_START_PLAYER){
		//Center the player horizontally.
  		player.fx=obj->getBox().x+(obj->getBox().w-player.getBox().w)/2;
		player.fy=obj->getBox().y;
		//Now reset the player to get him to it's new start position.
		player.reset(true);
	}
	if(obj->type==TYPE_START_SHADOW){
		//Center the shadow horizontally.
  		shadow.fx=obj->getBox().x+(obj->getBox().w-shadow.getBox().h)/2;
		shadow.fy=obj->getBox().y;
		//Now reset the shadow to get him to it's new start position.
		shadow.reset(true);
	}
}

void LevelEditor::removeObject(GameObject* obj){
	std::vector<GameObject*>::iterator it;
	std::map<Block*,vector<GameObject*> >::iterator mapIt;

	//Increase totalCollectables everytime we add a new collectable
	if(obj->type==TYPE_COLLECTABLE){
		totalCollectables--;
	}

	//Check if the object is in the selection.
	it=find(selection.begin(),selection.end(),obj);
	if(it!=selection.end()){
		//It is so we delete it.
		selection.erase(it);
	}

	//Check if the object is in the triggers.
	mapIt=triggers.find(dynamic_cast<Block*>(obj));
	if(mapIt!=triggers.end()){
		//It is so we remove it.
		triggers.erase(mapIt);
	}

	//Boolean if it could be a target.
	if(obj->type==TYPE_MOVING_BLOCK || obj->type==TYPE_MOVING_SHADOW_BLOCK || obj->type==TYPE_MOVING_SPIKES
		|| obj->type==TYPE_CONVEYOR_BELT || obj->type==TYPE_SHADOW_CONVEYOR_BELT || obj->type==TYPE_PORTAL){
		for(mapIt=triggers.begin();mapIt!=triggers.end();++mapIt){
			//Now loop the target vector.
			for(unsigned int o=0;o<(*mapIt).second.size();o++){
				//Check if the obj is in the target vector.
				if((*mapIt).second[o]==obj){
					(*mapIt).second.erase(find((*mapIt).second.begin(),(*mapIt).second.end(),obj));
					o--;
				}
			}
		}
	}

	//Check if the object is in the movingObjects.
	std::map<Block*,vector<MovingPosition> >::iterator movIt;
	movIt=movingBlocks.find(dynamic_cast<Block*>(obj));
	if(movIt!=movingBlocks.end()){
		//It is so we remove it.
		movingBlocks.erase(movIt);
	}

	//Check if the block isn't being configured with a window one way or another.
	std::map<GUIObject*,GameObject*>::iterator confIt;
	for(confIt=objectWindows.begin();confIt!=objectWindows.end();++confIt){
		if((*confIt).second==obj){
			destroyWindow((*confIt).first);
		}
	}

	//Now we remove the object from the levelObjects.
	{
		std::vector<Block*>::iterator it;
		it=find(levelObjects.begin(),levelObjects.end(),dynamic_cast<Block*>(obj));
		if(it!=levelObjects.end()){
			levelObjects.erase(it);
		}
	}
	
	delete obj;
	obj=NULL;

	//Set dirty of selection popup
	if(selectionPopup!=NULL) selectionPopup->dirty=true;
}

void LevelEditor::GUIEventCallback_OnEvent(std::string name,GUIObject* obj,int eventType){
	//Check if one of the windows is closed.
	if(eventType==GUIEventClick && (name=="lvlSettingsWindow" || name=="notificationBlockWindow" || name=="conveyorBlockWindow" || name=="scriptingWindow" || name=="levelScriptingWindow")){
		destroyWindow(obj);
		return;
	}
	//TODO: Add resize code for each GUIWindow.
	if(name=="lvlSettingsWindow"){
		return;
	}
	if(name=="notificationBlockWindow"){
		return;
	}
	if(name=="conveyorBlockWindow"){
		return;
	}
	if(name=="scriptingWindow"){
		return;
	}
	if(name=="levelScriptingWindow"){
		return;
	}
	
	//Check for GUI events.
	//Notification block configure events.
	if(name=="cfgNotificationBlockOK"){
		//Get the configuredObject.
		GameObject* configuredObject=objectWindows[obj];
		if(configuredObject){
			//Get the message textbox from the GUIWindow.
			GUITextArea* message=(GUITextArea*)obj->getChild("message");

			if(message){
				//Set the message of the notification block.
				configuredObject->setEditorProperty("message",message->getString());
			}
		}
	}
	//Conveyor belt block configure events.
	if(name=="cfgConveyorBlockOK"){
		//Get the configuredObject.
		GameObject* configuredObject=objectWindows[obj];
		if(configuredObject){
			//Get the speed textbox from the GUIWindow.
			GUISpinBox* speed=(GUISpinBox*)obj->getChild("speed");

			if(speed){
				//Set the speed of the conveyor belt.
				configuredObject->setEditorProperty("speed",speed->caption);
			}
		}
	}
	//LevelSetting events.
	if(name=="lvlSettingsOK"){
		GUIObject* object=obj->getChild("name");
		if(object)
			levelName=object->caption;
		object=obj->getChild("theme");
		if(object)
			levelTheme=object->caption;

		//target time and recordings.
		GUISpinBox* object2=(GUISpinBox*)obj->getChild("time");
		if(object2){
			float number=atof(object2->caption.c_str());
			if(number<=0){
				levelTime=-1;
			}else{
				levelTime=int(number*40.0+0.5);
			}
		}

		object2=(GUISpinBox*)obj->getChild("recordings");
		if(object){
			float number=atof(object2->caption.c_str());
			if(number<=0){
				levelRecordings=-1;
			}else{
				levelRecordings=int(number);
			}
		}
	}
	//Level scripting window events.
	if(name=="cfgLevelScriptingEventType"){
		//Get the script textbox from the GUIWindow.
		GUISingleLineListBox* list=(GUISingleLineListBox*)obj->getChild("cfgLevelScriptingEventType");

		if(list){
			//Loop through the scripts.
			for(unsigned int i=0;i<list->item.size();i++){
				GUIObject* script=obj->getChild(list->item[i].first);
				if(script){
					script->visible=(script->name==list->item[list->value].first);
					script->enabled=(script->name==list->item[list->value].first);
				}
			}
		}
		return;
	}
	if(name=="cfgLevelScriptingOK"){
		//Get the script textbox from the GUIWindow.
		GUISingleLineListBox* list=(GUISingleLineListBox*)obj->getChild("cfgLevelScriptingEventType");

		if(list){
			//Loop through the scripts.
			for(unsigned int i=0;i<list->item.size();i++){
				//Get the GUITextArea.
				GUITextArea* script=dynamic_cast<GUITextArea*>(obj->getChild(list->item[i].first));
				if(script){
					//Set the script for the target block.
					string str=script->getString();
					if(str.empty())
						scripts.erase(levelEventNameMap[script->name]);
					else
						scripts[levelEventNameMap[script->name]]=str;
				}
			}
		}
	}
	//Scripting window events.
	if(name=="cfgScriptingEventType"){
		//TODO: Save any unsaved scripts? (Or keep track of all scripts and save upon cfgScriptingOK?)
		//Get the configuredObject.
		Block* configuredObject=dynamic_cast<Block*>(objectWindows[obj]);
		if(configuredObject){
			//Get the script textbox from the GUIWindow.
			GUISingleLineListBox* list=(GUISingleLineListBox*)obj->getChild("cfgScriptingEventType");

			if(list){
				//Loop through the scripts.
				for(unsigned int i=0;i<list->item.size();i++){
					GUIObject* script=obj->getChild(list->item[i].first);
					if(script){
						script->visible=script->enabled=(script->name==list->item[list->value].first);
					}
				}
			}
		}
		return;
	}
	if(name=="cfgScriptingOK"){
		//Get the configuredObject.
		GameObject* configuredObject=objectWindows[obj];
		if(configuredObject){
			//Get the script textbox from the GUIWindow.
			GUISingleLineListBox* list=(GUISingleLineListBox*)obj->getChild("cfgScriptingEventType");
			GUIObject* id=obj->getChild("id");

			Block* block=dynamic_cast<Block*>(configuredObject);
			if(block){
				if(list){
					//Loop through the scripts.
					for(unsigned int i=0;i<list->item.size();i++){
						//Get the GUITextArea.
						GUITextArea* script=dynamic_cast<GUITextArea*>(obj->getChild(list->item[i].first));
						if(script){
							//Set the script for the target block.
							string str=script->getString();
							if(str.empty())
								block->scripts.erase(gameObjectEventNameMap[script->name]);
							else
								block->scripts[gameObjectEventNameMap[script->name]]=str;
						}
					}
				}
				if(id){
					//Set the new id for the target block.
					//TODO: Check for trigger links etc...
					(dynamic_cast<Block*>(configuredObject))->id=id->caption;
				}
			}
		}
	}

	//NOTE: We assume every event came from a window so remove it.
	destroyWindow(obj);
}

void LevelEditor::destroyWindow(GUIObject* window){
	//Make sure the given pointer isn't null.
	if(!window)
		return;
	
	//Remove the window from the GUIObject root.
	if(GUIObjectRoot){
		vector<GUIObject*>::iterator it;
		it=find(GUIObjectRoot->childControls.begin(),GUIObjectRoot->childControls.end(),window);
		if(it!=GUIObjectRoot->childControls.end()){
			GUIObjectRoot->childControls.erase(it);
		}
	}

	//Also remove the window from the objectWindows map.
	map<GUIObject*,GameObject*>::iterator it;
	it=objectWindows.find(window);
	if(it!=objectWindows.end()){
		objectWindows.erase(it);
	}
	
	//And delete the GUIWindow.
	delete window;
}

////////////////LOGIC////////////////////
void LevelEditor::logic(){
	if(playMode){
		//PlayMode so let the game do it's logic.
		Game::logic();
	}else{
		//In case of a selection or actions popup prevent the camera from moving.
		if(selectionPopup || actionsPopup)
			return;
		
		//Move the camera.
		if(cameraXvel!=0 || cameraYvel!=0){
			camera.x+=cameraXvel;
			camera.y+=cameraYvel;
			//Call the onCameraMove event.
			onCameraMove(cameraXvel,cameraYvel);
		}
		
		//Move the camera with the mouse.
		//Get the mouse location.
		int x,y;
		SDL_GetMouseState(&x,&y);
		SDL_Rect mouse={x,y,0,0};
		{
			//Check if the mouse isn't above a GUIObject (window).
			bool inside=false;
			for(unsigned int i=0;i<GUIObjectRoot->childControls.size();i++){
				SDL_Rect box={0,0,0,0};
				box.x=GUIObjectRoot->childControls[i]->left;
				box.y=GUIObjectRoot->childControls[i]->top;
				box.w=GUIObjectRoot->childControls[i]->width;
				box.h=GUIObjectRoot->childControls[i]->height;
				if(checkCollision(mouse,box))
					inside=true;
			}

			if(!inside){
				SDL_Rect r[3]={toolbarRect,toolboxRect};
				int m=2;
				//TODO: Also call onCameraMove when moving using the mouse.
				setCamera(r,m);
			}
		}

		//It isn't playMode so the mouse should be checked.
		tooltip=-1;

		//We loop through the number of tools + the number of buttons.
		for(int t=0; t<NUMBER_TOOLS+6; t++){
			SDL_Rect toolRect={(SCREEN_WIDTH-410)/2+(t*40)+((t+1)*10),SCREEN_HEIGHT-45,40,40};

			//Check for collision.
			if(checkCollision(mouse,toolRect)==true){
				//Set the tooltip tool.
				tooltip=t;
			}
		}
	}
}

/////////////////RENDER//////////////////////
void LevelEditor::render(){
	//Always let the game render the game.
	Game::render();

	//Only render extra stuff like the toolbar, selection, etc.. when not in playMode.
	if(!playMode){
		//Render the selectionmarks.
		//TODO: Check if block is in sight.
		for(unsigned int o=0; o<selection.size(); o++){
			//Get the location to draw.
			SDL_Rect r=selection[o]->getBox();
			r.x-=camera.x;
			r.y-=camera.y;
			
			drawGUIBox(r.x,r.y,r.w,r.h,screen,0xFFFFFF33);

			//Draw the selectionMarks.
			applySurface(r.x,r.y,selectionMark,screen,NULL);
			applySurface(r.x+r.w-5,r.y,selectionMark,screen,NULL);
			applySurface(r.x,r.y+r.h-5,selectionMark,screen,NULL);
			applySurface(r.x+r.w-5,r.y+r.h-5,selectionMark,screen,NULL);
		}

		//Clear the placement surface.
		SDL_FillRect(placement,NULL,0x00FF00FF);
		
		Uint32 color=SDL_MapRGB(placement->format,themeTextColor.r,themeTextColor.g,themeTextColor.b);

		//Draw the dark areas marking the outside of the level.
		SDL_Rect r;
		if(camera.x<0){
			//Draw left side.
			r.x=0;
			r.y=0;
			r.w=0-camera.x;
			r.h=SCREEN_HEIGHT;
			SDL_FillRect(placement,&r,color);
		}
		if(camera.x>LEVEL_WIDTH-SCREEN_WIDTH){
			//Draw right side.
			r.x=LEVEL_WIDTH-camera.x;
			r.y=0;
			r.w=SCREEN_WIDTH-(LEVEL_WIDTH-camera.x);
			r.h=SCREEN_HEIGHT;
			SDL_FillRect(placement,&r,color);
		}
		if(camera.y<0){
			//Draw the top.
			r.x=0;
			r.y=0;
			r.w=SCREEN_WIDTH;
			r.h=0-camera.y;
			SDL_FillRect(placement,&r,color);
		}
		if(camera.y>LEVEL_HEIGHT-SCREEN_HEIGHT){
			//Draw the bottom.
			r.x=0;
			r.y=LEVEL_HEIGHT-camera.y;
			r.w=SCREEN_WIDTH;
			r.h=SCREEN_HEIGHT-(LEVEL_HEIGHT-camera.y);
			SDL_FillRect(placement,&r,color);
		}

		//Check if we should draw on the placement surface.
		showConfigure();
		if(selectionDrag){
			showSelectionDrag();
		}else{
			if(tool==ADD){
				showCurrentObject();
			}
		}
		
		//Get the current mouse location.
		int x,y;
		SDL_GetMouseState(&x,&y);
		//Create the rectangle.
		SDL_Rect mouse={x+camera.x,y+camera.y,0,0};
		
		//Find a block where the mouse is hovering on.
		for(unsigned int o=0; o<levelObjects.size(); o++){
			SDL_Rect rect=levelObjects[o]->getBox();
			if(checkCollision(rect,mouse)==true){
				if(tool==REMOVE){
					drawGUIBox(rect.x-camera.x,rect.y-camera.y,rect.w,rect.h,screen,0xFF000055);
					currentCursor=CURSOR_REMOVE;
				}else{
					drawGUIBox(rect.x-camera.x,rect.y-camera.y,rect.w,rect.h,screen,0xFFFFFF33);
				}
			}
		}

		//Draw the level borders.
		drawRect(-camera.x,-camera.y,LEVEL_WIDTH,LEVEL_HEIGHT,screen);

		//Render the placement surface.
		applySurface(0,0,placement,screen,NULL);

		//Render the hud layer.
		renderHUD();

		//Render selection popup (if any).
		if(selectionPopup!=NULL){
			if(linking || moving){
				//If we switch to linking mode then delete it
				//FIXME: Logic in the render method.
				delete selectionPopup;
				selectionPopup=NULL;
			}else{
				selectionPopup->render();
			}
		}

		//Render actions popup (if any).
		if(actionsPopup!=NULL){
			actionsPopup->render();
		}
	}
}

void LevelEditor::renderHUD(){
	//If moving show the moving speed in the top right corner.
	if(moving){
		//Calculate width of text "Movespeed: 100" to keep the same position with every value
		if (movingSpeedWidth==-1){
			int w;
			TTF_SizeUTF8(fontText,tfm::format(_("Movespeed: %s"),100).c_str(),&w,NULL);
			movingSpeedWidth=w+4;
		}
	
		//Now render the text.
		SDL_Color black={0,0,0,0};
		SDL_Surface* bm=TTF_RenderUTF8_Blended(fontText,tfm::format(_("Movespeed: %s"),movingSpeed).c_str(),black);

		//Draw the text in box and free the surface.
		drawGUIBox(SCREEN_WIDTH-movingSpeedWidth-2,-2,movingSpeedWidth+8,bm->h+6,screen,0xFFFFFFFF);
		applySurface(SCREEN_WIDTH-movingSpeedWidth,2,bm,screen,NULL);
		SDL_FreeSurface(bm);
	}

	//On top of all render the toolbar.
	drawGUIBox(toolbarRect.x,toolbarRect.y,8*50+10,52,screen,0xEDEDEDFF);
	//Draw the first four options.
	SDL_Rect r={0,0,200,50};
	applySurface(toolbarRect.x+5,toolbarRect.y,toolbar,screen,&r);
	//And the last three.
	r.x=200;
	r.w=150;
	applySurface(toolbarRect.x+255,toolbarRect.y,toolbar,screen,&r);
	
	//Now render a tooltip.
	if(tooltip>=0){
		//The back and foreground colors.
		SDL_Color fg={0,0,0};

		//Tool specific text.
		SDL_Surface* tip=NULL;
		switch(tooltip){
			case 0:
				tip=TTF_RenderUTF8_Blended(fontText,_("Select"),fg);
				break;
			case 1:
				tip=TTF_RenderUTF8_Blended(fontText,_("Add"),fg);
				break;
			case 2:
				tip=TTF_RenderUTF8_Blended(fontText,_("Delete"),fg);
				break;
			case 3:
				tip=TTF_RenderUTF8_Blended(fontText,_("Play"),fg);
				break;
			case 5:
				tip=TTF_RenderUTF8_Blended(fontText,_("Level settings"),fg);
				break;
			case 6:
				tip=TTF_RenderUTF8_Blended(fontText,_("Save level"),fg);
				break;
			case 7:
				tip=TTF_RenderUTF8_Blended(fontText,_("Back to menu"),fg);
				break;
			default:
				break;
		}

		//Draw only if there's a tooltip available
		if(tip!=NULL){
			SDL_Rect r={(SCREEN_WIDTH-390)/2+(tooltip*40)+(tooltip*10),SCREEN_HEIGHT-45,40,40};
			r.y=SCREEN_HEIGHT-50-tip->h;
			if(r.x+tip->w>SCREEN_WIDTH-50)
				r.x=SCREEN_WIDTH-50-tip->w;

			//Draw borders around text
			Uint32 color=0xFFFFFF00|230;
			drawGUIBox(r.x-2,r.y-2,tip->w+4,tip->h+4,screen,color);

			//Draw tooltip's text
			SDL_BlitSurface(tip,NULL,screen,&r);
			SDL_FreeSurface(tip);
		}
	}

	//Render the tool box.
	if(!playMode && !moving && tool==ADD && selectionPopup==NULL && actionsPopup==NULL && objectWindows.empty()){
		if(toolboxVisible){
			toolboxRect.x=0;
			toolboxRect.y=0;
			toolboxRect.w=SCREEN_WIDTH;
			toolboxRect.h=64;

			drawGUIBox(-2,-2,SCREEN_WIDTH+4,66,screen,0xFFFFFF00|230);

			//Draw the hide icon.
			SDL_Rect r={SCREEN_WIDTH-20,2,0,0};
			SDL_Rect r2={80,0,16,16};
			r.x=SCREEN_WIDTH-20;
			SDL_BlitSurface(bmGUI,&r2,screen,&r);

			//Calculate the maximal number of blocks can be displayed.
			int m=(SCREEN_WIDTH-48)/64;
			if(toolboxIndex>=EDITOR_ORDER_MAX-m){
				toolboxIndex=EDITOR_ORDER_MAX-m;
			}else{
				//Draw an icon.
				r.x=SCREEN_WIDTH-20;
				r.y=24;
				r2.x=96;
				r2.y=16;
				SDL_BlitSurface(bmGUI,&r2,screen,&r);
			}
			if(toolboxIndex<=0){
				toolboxIndex=0;
			}else{
				//Draw an icon.
				r.x=4;
				r.y=24;
				r2.x=80;
				r2.y=16;
				SDL_BlitSurface(bmGUI,&r2,screen,&r);
			}

			//Draw available blocks.
			for(int i=0;i<m;i++){
				if(i+toolboxIndex>=EDITOR_ORDER_MAX) break;

				//Draw a rectangle around the current tool.
				if(i+toolboxIndex==currentType){
					drawGUIBox(i*64+24,3,64,58,screen,0xDDDDDDFF);
				}

				ThemeBlock* obj=objThemes.getBlock(editorTileOrder[i+toolboxIndex]);
				if(obj){
					obj->editorPicture.draw(screen,i*64+24+7,7);
				}
			}

			//Draw a tool tip.
			int x,y;
			SDL_GetMouseState(&x,&y);
			if(y<64 && x>=24 && x<24+m*64){
				int i=(x-24)/64;
				if(i+toolboxIndex<EDITOR_ORDER_MAX){
					SDL_Color fg={0,0,0};
					SDL_Surface* tip=TTF_RenderUTF8_Blended(fontText,_(blockNames[editorTileOrder[i+toolboxIndex]]),fg);

					SDL_Rect r={24+i*64,64,40,40};
					if(r.x+tip->w>SCREEN_WIDTH-50)
						r.x=SCREEN_WIDTH-50-tip->w;

					//Draw borders around text
					Uint32 color=0xFFFFFF00|230;
					drawGUIBox(r.x-2,r.y-2,tip->w+4,tip->h+4,screen,color);

					//Draw tooltip's text
					SDL_BlitSurface(tip,NULL,screen,&r);
					SDL_FreeSurface(tip);
				}
			}
		}else{
			SDL_Color fg={0,0,0};
			SDL_Surface* tip=TTF_RenderUTF8_Blended(fontText,_("Toolbox"),fg);

			toolboxRect.x=SCREEN_WIDTH-tip->w-28;
			toolboxRect.y=0;
			toolboxRect.w=tip->w+28;
			toolboxRect.h=tip->h+4;

			SDL_Rect r={SCREEN_WIDTH-tip->w-24,2,0,0};
			drawGUIBox(r.x-4,-2,tip->w+32,tip->h+6,screen,0xFFFFFFFF);

			SDL_BlitSurface(tip,NULL,screen,&r);
			SDL_FreeSurface(tip);

			SDL_Rect r2={96,0,16,16};
			r.x=SCREEN_WIDTH-20;
			SDL_BlitSurface(bmGUI,&r2,screen,&r);
		}
	}else{
		toolboxRect.x=-1;
		toolboxRect.y=-1;
		toolboxRect.w=0;
		toolboxRect.h=0;
	}

	//Draw a rectangle around the current tool.
	Uint32 color=0xFFFFFF00;
	drawGUIBox((SCREEN_WIDTH-390)/2+(tool*40)+(tool*10),SCREEN_HEIGHT-46,42,42,screen,color);
}

void LevelEditor::showCurrentObject(){
	//Get the current mouse location.
	int x,y;
	SDL_GetMouseState(&x,&y);
	x+=camera.x;
	y+=camera.y;

	//Check if we should snap the block to grid or not.
	if(!pressedShift){
		snapToGrid(&x,&y);
	}else{
		x-=25;
		y-=25;
	}

	//Check if the currentType is a legal type.
	if(currentType>=0 && currentType<EDITOR_ORDER_MAX){
		ThemeBlock* obj=objThemes.getBlock(editorTileOrder[currentType]);
		if(obj){
			obj->editorPicture.draw(placement,x-camera.x,y-camera.y);
		}
	}
}

void LevelEditor::showSelectionDrag(){
	//Get the current mouse location.
	int x,y;
	SDL_GetMouseState(&x,&y);
	//Create the rectangle.
	x+=camera.x;
	y+=camera.y;

	//Check if we should snap the block to grid or not.
	if(!pressedShift){
		snapToGrid(&x,&y);
	}else{
		x-=25;
		y-=25;
	}

	//Check if the drag center isn't null.
	if(dragCenter==NULL) return;
	//The location of the dragCenter.
	SDL_Rect r=dragCenter->getBox();

	//Loop through the selection.
	//TODO: Check if block is in sight.
	for(unsigned int o=0; o<selection.size(); o++){
		ThemeBlock* obj=objThemes.getBlock(selection[o]->type);
		if(obj){
			SDL_Rect r1=selection[o]->getBox();
			obj->editorPicture.draw(placement,(r1.x-r.x)+x-camera.x,(r1.y-r.y)+y-camera.y);
		}
	}
}

void LevelEditor::showConfigure(){
	//arrow animation value. go through 0-65535 and loops.
	static unsigned short arrowAnimation=0;
	arrowAnimation++;
	
	//By default use black color for arrows.
	Uint32 color=0x00000000;
	
	//Theme can change the color.
	//TODO: use the actual color from the theme.
	if(themeTextColor.r>128 && themeTextColor.g>128 && themeTextColor.b>128)
		color=0xffffffff;

	//Draw the trigger lines.
	{
		map<Block*,vector<GameObject*> >::iterator it;
		for(it=triggers.begin();it!=triggers.end();++it){
			//Check if the trigger has linked targets.
			if(!(*it).second.empty()){
				//The location of the trigger.
				SDL_Rect r=(*it).first->getBox();

				//Loop through the targets.
				for(unsigned int o=0;o<(*it).second.size();o++){
					//Get the location of the target.
					SDL_Rect r1=(*it).second[o]->getBox();

					//Draw the line from the center of the trigger to the center of the target.
					drawLineWithArrow(r.x-camera.x+25,r.y-camera.y+25,r1.x-camera.x+25,r1.y-camera.y+25,placement,color,32,arrowAnimation%32);

					//Also draw two selection marks.
					applySurface(r.x-camera.x+25-2,r.y-camera.y+25-2,selectionMark,screen,NULL);
					applySurface(r1.x-camera.x+25-2,r1.y-camera.y+25-2,selectionMark,screen,NULL);
				}
			}
		}

		//Draw a line to the mouse from the linkingTrigger when linking.
		if(linking){
			//Get the current mouse location.
			int x,y;
			SDL_GetMouseState(&x,&y);

			//Draw the line from the center of the trigger to mouse.
			drawLineWithArrow(linkingTrigger->getBox().x-camera.x+25,linkingTrigger->getBox().y-camera.y+25,x,y,placement,color,32,arrowAnimation%32);
		}
	}

	//Draw the moving positions.
	map<Block*,vector<MovingPosition> >::iterator it;
	for(it=movingBlocks.begin();it!=movingBlocks.end();++it){
		//Check if the block has positions.
		if(!(*it).second.empty()){
			//The location of the moving block.
			SDL_Rect block=(*it).first->getBox();
			block.x+=25-camera.x;
			block.y+=25-camera.y;

			//The location of the previous position.
			//The first time it's the moving block's position self.
			SDL_Rect r=block;

			//Loop through the positions.
			for(unsigned int o=0;o<(*it).second.size();o++){
				//Draw the line from the center of the previous position to the center of the position.
				//x and y are the coordinates for the current moving position.
				int x=block.x+(*it).second[o].x;
				int y=block.y+(*it).second[o].y;

				//Check if we need to draw line
				double dx=r.x-x;
				double dy=r.y-y;
				double d=sqrt(dx*dx+dy*dy);
				if(d>0.001f){
					if(it->second[o].time>0){
						//Calculate offset to contain the moving speed.
						int offset=int(d*arrowAnimation/it->second[o].time)%32;
						drawLineWithArrow(r.x,r.y,x,y,placement,color,32,offset);
					}else{
						//time==0 ???? so don't draw arrow at all
						drawLine(r.x,r.y,x,y,placement);
					}
				}

				//And draw a marker at the end.
				applySurface(x-13,y-13,movingMark,screen,NULL);

				//Get the box of the previous position.
				SDL_Rect tmp={x,y,0,0};
				r=tmp;
			}
		}
	}

	//Draw a line to the mouse from the previous moving pos.
	if(moving){
		//Get the current mouse location.
		int x,y;
		SDL_GetMouseState(&x,&y);

		//Check if we should snap the block to grid or not.
		if(!pressedShift){
			x+=camera.x;
			y+=camera.y;
			snapToGrid(&x,&y);
			x-=camera.x;
			y-=camera.y;
		}else{
			x-=25;
			y-=25;
		}

		int posX,posY;

		//Check if there are moving positions for the moving block.
		if(!movingBlocks[movingBlock].empty()){
			//Draw the line from the center of the previouse moving positions to mouse.
			posX=movingBlocks[movingBlock].back().x;
			posY=movingBlocks[movingBlock].back().y;

			posX-=camera.x;
			posY-=camera.y;

			posX+=movingBlock->getBox().x;
			posY+=movingBlock->getBox().y;
		}else{
			//Draw the line from the center of the movingblock to mouse.
			posX=movingBlock->getBox().x-camera.x;
			posY=movingBlock->getBox().y-camera.y;
		}

		//Calculate offset to contain the moving speed.
		int offset=int(double(arrowAnimation)*movingSpeed/10.0)%32;

		drawLineWithArrow(posX+25,posY+25,x+25,y+25,placement,color,32,offset);
		applySurface(x+12,y+12,movingMark,screen,NULL);
	}

}

void LevelEditor::resize(){
	//Call the resize method of the Game.
	Game::resize();

	//Now update the placement surface.
	if(placement)
		SDL_FreeSurface(placement);
	placement=SDL_CreateRGBSurface(SDL_SWSURFACE|SDL_SRCALPHA,SCREEN_WIDTH,SCREEN_HEIGHT,32,RMASK,GMASK,BMASK,0);
	SDL_SetColorKey(placement,SDL_SRCCOLORKEY|SDL_RLEACCEL,SDL_MapRGB(placement->format,255,0,255));
	SDL_SetAlpha(placement,SDL_SRCALPHA,125);
	
	//Move the toolbar's position rect used for collision.
	toolbarRect.x=(SCREEN_WIDTH-410)/2;
	toolbarRect.y=SCREEN_HEIGHT-50;
}

//Filling the order array
const int LevelEditor::editorTileOrder[EDITOR_ORDER_MAX]={
	TYPE_BLOCK,
	TYPE_SHADOW_BLOCK,
	TYPE_SPIKES,
	TYPE_FRAGILE,
	TYPE_MOVING_BLOCK,
	TYPE_MOVING_SHADOW_BLOCK,
	TYPE_MOVING_SPIKES,
	TYPE_CONVEYOR_BELT,
	TYPE_SHADOW_CONVEYOR_BELT,
	TYPE_BUTTON,
	TYPE_SWITCH,
	TYPE_PORTAL,
	TYPE_SWAP,
	TYPE_CHECKPOINT,
	TYPE_NOTIFICATION_BLOCK,
	TYPE_START_PLAYER,
	TYPE_START_SHADOW,
	TYPE_EXIT,
	TYPE_COLLECTABLE,
	TYPE_PUSHABLE
};
