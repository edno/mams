/****************************************************************************
** Copyright (C) 2011 Luka Horvat <redreaper132 at gmail.com>
** Copyright (C) 2011 Edward Lii <edward_iii at myway.com>
** Copyright (C) 2011 O. Bahri Gordebak <gordebak at gmail.com>
**
**
** This file may be used under the terms of the GNU General Public
** License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/
#include "Functions.h"
#include "GameState.h"
#include "Globals.h"
#include "Objects.h"
#include "GUIOverlay.h"

using namespace std;

GUIOverlay::GUIOverlay(GUIObject* root,bool dim):root(root),dim(dim){
	//First keep the pointer to the current GUIObjectRoot and currentState.
	parentState=currentState;
	tempGUIObjectRoot=GUIObjectRoot;

	//Now set the GUIObject root to the new root.
	currentState=this;
	GUIObjectRoot=root;
	
	//Dim the background.
	if(dim){
		SDL_FillRect(tempSurface,NULL,0);
		SDL_SetAlpha(tempSurface,SDL_SRCALPHA,155);
		SDL_BlitSurface(tempSurface,NULL,screen,NULL);
	}
}

GUIOverlay::~GUIOverlay(){
	//We need to place everything back.
	currentState=parentState;
	parentState=NULL;

	//Delete the GUI if present.
	if(GUIObjectRoot)
		delete GUIObjectRoot;

	//Now put back the parent gui.
	GUIObjectRoot=tempGUIObjectRoot;
	tempGUIObjectRoot=NULL;
}

void GUIOverlay::enterLoop(){
	while(GUIObjectRoot){
		while(SDL_PollEvent(&event)){
			//Check for a resize event.
			if(event.type==SDL_VIDEORESIZE){
				onVideoResize();
				continue;
			}
			GUIObjectHandleEvents(true);
			
			//Also check for the return, escape or backspace button.
			//escape = KEYUP.
			//backspace and return = KEYDOWN.
// 			if(((event.type==SDL_KEYUP && event.key.keysym.sym==SDLK_ESCAPE) ||
// 				(event.type==SDL_KEYDOWN && (event.key.keysym.sym==SDLK_RETURN || event.key.keysym.sym==SDLK_BACKSPACE)))){
// 				delete root;
// 				root=NULL;
// 			}

		}
		//Render the gui.
		if(GUIObjectRoot)
			GUIObjectRoot->render();
		flipScreen();
		SDL_Delay(30);
	}

	//We broke out so clean up.
	delete this;
}

void GUIOverlay::handleEvents(){
	//Check if we need to quit, if so we enter the exit state.
	if(event.type==SDL_QUIT){
		setNextState(STATE_EXIT);
	}
}

//Nothing to do here
void GUIOverlay::logic(){
	//Check if the GUIObjectRoot (of the overlay) is deleted.
	if(!GUIObjectRoot)
		delete this;
}
void GUIOverlay::render(){}


void GUIOverlay::resize(){
	//We recenter the GUI.
	GUIObjectRoot->left=(SCREEN_WIDTH-GUIObjectRoot->width)/2;
	GUIObjectRoot->top=(SCREEN_HEIGHT-GUIObjectRoot->height)/2;

	//Now let the parent state resize.
	GUIObjectRoot=tempGUIObjectRoot;
	parentState->resize();
	//NOTE: After the resize it's likely that the GUIObjectRoot is new so we need to update our tempGUIObjectRoot pointer.
	tempGUIObjectRoot=GUIObjectRoot;

	//Now render the parentState.
	parentState->render();
	if(GUIObjectRoot)
		GUIObjectRoot->render();

	//And set the GUIObjectRoot back to the overlay gui.
	GUIObjectRoot=root;

	//Dim the background.
	if(dim){
		SDL_FillRect(tempSurface,NULL,0);
		SDL_SetAlpha(tempSurface,SDL_SRCALPHA,155);
		SDL_BlitSurface(tempSurface,NULL,screen,NULL);
	}
}