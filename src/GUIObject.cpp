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

#include "GUIObject.h"
#include <iostream>
#include <list>
using namespace std;

//Set the GUIObjectRoot to NULL.
GUIObject* GUIObjectRoot=NULL;
//Initialise the event queue.
list<GUIEvent> GUIEventQueue;

void GUIObjectHandleEvents(bool kill){
	//Check if user resizes the window.
	if(event.type==SDL_VIDEORESIZE){
		//onVideoResize();

		//Don't let other objects process this event (?)
		return;
	}

	//Make sure that GUIObjectRoot isn't null.
	if(GUIObjectRoot)
		GUIObjectRoot->handleEvents();
	
	//Check for SDL_QUIT.
	if(event.type==SDL_QUIT && kill){
		//We get a quit event so enter the exit state.
		setNextState(STATE_EXIT);
		delete GUIObjectRoot;
		GUIObjectRoot=NULL;
		return;
	}
	
	//Keep calling events until there are none left.
	while(!GUIEventQueue.empty()){
		//Get one event and remove it from the queue.
		GUIEvent e=GUIEventQueue.front();
		GUIEventQueue.pop_front();
		
		//If an eventCallback exist call it.
		if(e.eventCallback){
			e.eventCallback->GUIEventCallback_OnEvent(e.name,e.obj,e.eventType);
		}
	}
	//We empty the event queue just to be sure.
	GUIEventQueue.clear();
}

GUIObject::~GUIObject(){
	//The cache is used as the actual image for GUIObjectImage and shouldn't be freed.
	if(cache){
		SDL_FreeSurface(cache);
		cache=NULL;
	}
	//We need to delete every child we have.
	for(unsigned int i=0;i<childControls.size();i++){
		delete childControls[i];
	}
	//Deleted the childs now empty the childControls vector.
	childControls.clear();
}

bool GUIObject::handleEvents(int x,int y,bool enabled,bool visible,bool processed){
	//Boolean if the event is processed.
	bool b=processed;
	
	//The GUIObject is only enabled when its parent are enabled.
	enabled=enabled && this->enabled;
	//The GUIObject is only enabled when its parent are enabled.
	visible=visible && this->visible;
	
	//Get the absolute position.
	x+=left-gravityX;
	y+=top;
	
	//Also let the children handle their events.
	for(unsigned int i=0;i<childControls.size();i++){
		bool b1=childControls[i]->handleEvents(x,y,enabled,visible,b);
		
		//The event is processed when either our or the childs is true (or both).
		b=b||b1;
	}
	return b;
}

void GUIObject::render(int x,int y,bool draw){
	//There's no need drawing the GUIObject when it's invisible.
	if(!visible)
		return;
	
	//Get the absolute x and y location.
	x+=left;
	y+=top;
	
	//We now need to draw all the children of the GUIObject.
	for(unsigned int i=0;i<childControls.size();i++){
		childControls[i]->render(x,y,draw);
	}
}

//////////////GUIButton///////////////////////////////////////////////////////////////////

bool GUIButton::handleEvents(int x,int y,bool enabled,bool visible,bool processed){
	//Boolean if the event is processed.
	bool b=processed;
	
	//The widget is only enabled when its parent are enabled.
	enabled=enabled && this->enabled;
	//The widget is only enabled when its parent are enabled.
	visible=visible && this->visible;
	
	//Get the absolute position.
	x+=left-gravityX;
	y+=top;
	
	//Set state to 0.
	state=0;
	
	//Only check for events when the object is both enabled and visible.
	if(enabled&&visible){
		//The mouse location (x=i, y=j) and the mouse button (k).
		int i,j,k;
		k=SDL_GetMouseState(&i,&j);
		
		//Check if the mouse is inside the widget.
		if(i>=x&&i<x+width&&j>=y&&j<y+height){
			//We have hover so set state to one.
			state=1;
			//Check for a mouse button press.
			if(k&SDL_BUTTON(1))
				state=2;
			
			//Check if there's a mouse press and the event hasn't been already processed.
			if(event.type==SDL_MOUSEBUTTONUP && event.button.button==SDL_BUTTON_LEFT && !b){
				//If event callback is configured then add an event to the queue.
				if(eventCallback){
					GUIEvent e={eventCallback,name,this,GUIEventClick};
					GUIEventQueue.push_back(e);
				}
				
				//Event has been processed.
				b=true;
			}
		}
	}
	
	//Also let the children handle their events.
	for(unsigned int i=0;i<childControls.size();i++){
		bool b1=childControls[i]->handleEvents(x,y,enabled,visible,b);
		
		//The event is processed when either our or the childs is true (or both).
		b=b||b1;
	}
	return b;
}

//Found on gmane.comp.lib.sdl mailing list, see: http://comments.gmane.org/gmane.comp.lib.sdl/33664
//Original code by "Patricia Curtis" and later modified by "Jason"
static void SetSurfaceTrans(SDL_Surface* Src,double PercentTrans){
	Uint8 Sbpp = Src->format->BytesPerPixel;
	Uint8 *Sbits;
	
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	int amask = 0x000000ff;
	int cmask = 0xffffff00;
#else
	int amask = 0xff000000;
	int cmask = 0x00ffffff;
	int Shift = 24;
#endif
	
	int x,y;
	Uint32 Pixels;
	Uint32 Alpha;
	
	for(y=0;y<Src->h;y++)
	{
		for(x=0;x<Src->w;x++)
		{
			Sbits = ((Uint8 *)Src->pixels+(y*Src->pitch)+(x*Sbpp));
			Pixels = *((Uint32 *)(Sbits));
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			Alpha = Pixels & mask;
#else
			Alpha = (Pixels&amask)>>Shift;
#endif
			Alpha*=PercentTrans;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			*((Uint32 *)(Sbits)) = (Pixels & cmask)|Alpha;
#else
			*((Uint32 *)(Sbits)) = (Pixels & cmask)|(Alpha<<Shift);
#endif
		}
	}
}

void GUIButton::render(int x,int y,bool draw){
	//There's no need drawing the widget when it's invisible.
	if(!visible)
		return;
	
	//Rectangle the size of the widget.
	SDL_Rect r;
	
	//Get the absolute x and y location.
	x+=left;
	y+=top;
	
	//Check if the enabled state changed or the caption, if so we need to clear the (old) cache.
	if(enabled!=cachedEnabled || caption.compare(cachedCaption)!=0 || width<=0){
		//Free the cache.
		SDL_FreeSurface(cache);
		cache=NULL;
		
		//And cache the new values.
		cachedEnabled=enabled;
		cachedCaption=caption;
		
		//Finally resize the widget
		if(autoWidth)
			width=-1;
	}
	
	//Get the text and make sure it isn't empty.
	const char* lp=caption.c_str();
	if(lp!=NULL && lp[0]){
		//Update cache if needed.
		if(!cache){
			SDL_Color color;
			if(inDialog)
				color=themeTextColorDialog;
			else
				color=themeTextColor;
					
			if(!smallFont)
				cache=TTF_RenderUTF8_Blended(fontGUI,lp,color);
			else
				cache=TTF_RenderUTF8_Blended(fontGUISmall,lp,color);
			
			//Make the widget transparent if it's disabled.
			if(!enabled)
				SetSurfaceTrans(cache,0.5);
			
			//Calculate proper size for the widget.
			if(width<=0){
				width=cache->w+50;
				if(gravity==GUIGravityCenter){
					gravityX=int(width/2);
				}else if(gravity==GUIGravityRight){
					gravityX=width;
				}else{
					gravityX=0;
				}
			}
		}
		
		if(draw){
			//Center the text both vertically as horizontally.
			r.x=x-gravityX+(width-cache->w)/2;
			r.y=y+(height-cache->h)/2-GUI_FONT_RAISE;
		
			//Check if the arrows don't fall of.
			if(cache->w+32<=width){
				//Create a rectangle that selects the right image from bmGUI.
				SDL_Rect r2={64,0,16,16};
				if(state==1){
					if(inDialog){
						applySurface(x-gravityX+(width-cache->w)/2+4+cache->w+5,y+2,arrowLeft2,screen,NULL);
						applySurface(x-gravityX+(width-cache->w)/2-25,y+2,arrowRight2,screen,NULL);
					}else{
						applySurface(x-gravityX+(width-cache->w)/2+4+cache->w+5,y+2,arrowLeft1,screen,NULL);
						applySurface(x-gravityX+(width-cache->w)/2-25,y+2,arrowRight1,screen,NULL);
					}
				}else if(state==2){
					if(inDialog){
						applySurface(x-gravityX+(width-cache->w)/2+4+cache->w,y+2,arrowLeft2,screen,NULL);
						applySurface(x-gravityX+(width-cache->w)/2-20,y+2,arrowRight2,screen,NULL);
					}else{
						applySurface(x-gravityX+(width-cache->w)/2+4+cache->w,y+2,arrowLeft1,screen,NULL);
						applySurface(x-gravityX+(width-cache->w)/2-20,y+2,arrowRight1,screen,NULL);
					}
				}
			}
			
			//Draw the text and free the surface.
			SDL_BlitSurface(cache,NULL,screen,&r);
		}
	}
}

//////////////GUICheckBox///////////////////////////////////////////////////////////////////

bool GUICheckBox::handleEvents(int x,int y,bool enabled,bool visible,bool processed){
	//Boolean if the event is processed.
	bool b=processed;
	
	//The widget is only enabled when its parent are enabled.
	enabled=enabled && this->enabled;
	//The widget is only enabled when its parent are enabled.
	visible=visible && this->visible;
	
	//Get the absolute position.
	x+=left-gravityX;
	y+=top;

	//Set state to 0.
	state=0;
	
	//Only check for events when the object is both enabled and visible.
	if(enabled&&visible){
		//The mouse location (x=i, y=j) and the mouse button (k).
		int i,j,k;
		k=SDL_GetMouseState(&i,&j);
	
		//Check if the mouse is inside the widget.
		if(i>=x&&i<x+width&&j>=y&&j<y+height){
			//We have hover so set state to one.
			state=1;
			//Check for a mouse button press.
			if(k&SDL_BUTTON(1))
				state=2;
			
			//Check if there's a mouse press and the event hasn't been already processed.
			if(event.type==SDL_MOUSEBUTTONUP && event.button.button==SDL_BUTTON_LEFT && !b){
				//It's a checkbox so toggle the value.
				value=value?0:1;
				
				//If event callback is configured then add an event to the queue.
				if(eventCallback){
					GUIEvent e={eventCallback,name,this,GUIEventClick};
					GUIEventQueue.push_back(e);
				}
				
				//Event has been processed.
				b=true;
			}
		}
	}
	
	return b;
}

void GUICheckBox::render(int x,int y,bool draw){
	//There's no need drawing the widget when it's invisible.
	if(!visible)
		return;
	
	//Get the absolute x and y location.
	x+=left;
	y+=top;
	
	//Check if the enabled state changed or the caption, if so we need to clear the (old) cache.
	if(enabled!=cachedEnabled || caption.compare(cachedCaption)!=0 || width<=0){
		//Free the cache.
		SDL_FreeSurface(cache);
		cache=NULL;
		
		//And cache the new values.
		cachedEnabled=enabled;
		cachedCaption=caption;
		
		//Finally resize the widget
		if(autoWidth)
			width=-1;
	}
	
	//Rectangle the size of the widget.
	SDL_Rect r;
	r.x=x;
	r.y=y;
	r.w=width;
	r.h=height;
	
	//Get the text.
	const char* lp=caption.c_str();
	//Make sure it isn't empty.
	if(lp!=NULL && lp[0]){
		//Update the cache if needed.
		if(!cache){
			SDL_Color color;
			if(inDialog)
				color=themeTextColorDialog;
			else
				color=themeTextColor;
		
			cache=TTF_RenderUTF8_Blended(fontText,lp,color);
		}
		
		if(draw){
			//Calculate the location, center it vertically.
			r.x=x;
			r.y=y+(height - cache->h)/2;
		
			//Draw the text and free the surface.
			SDL_BlitSurface(cache,NULL,screen,&r);
		}
	}
	
	if(draw){
		//Draw the check (or not).
		SDL_Rect r1={0,0,16,16};
		if(value==1||value==2)
			r1.x=value*16;
		r.x=x+width-20;
		r.y=y+(height-16)/2;
		SDL_BlitSurface(bmGUI,&r1,screen,&r);
	}
}

//////////////GUILabel///////////////////////////////////////////////////////////////////

bool GUILabel::handleEvents(int x,int y,bool enabled,bool visible,bool processed){
	return processed;
}

void GUILabel::render(int x,int y,bool draw){
	//There's no need drawing the widget when it's invisible.
	if(!visible)
		return;
	
	//Check if the enabled state changed or the caption, if so we need to clear the (old) cache.
	if(enabled!=cachedEnabled || caption.compare(cachedCaption)!=0 || width<=0){
		//Free the cache.
		SDL_FreeSurface(cache);
		cache=NULL;
		
		//And cache the new values.
		cachedEnabled=enabled;
		cachedCaption=caption;
		
		//Finally resize the widget
		if(autoWidth)
			width=-1;
	}
	
	//Get the absolute x and y location.
	x+=left;
	y+=top;
	
	//Rectangle the size of the widget.
	SDL_Rect r;
	r.x=x;
	r.y=y;
	r.w=width;
	r.h=height;
	
	//Get the caption and make sure it isn't empty.
	const char* lp=caption.c_str();
	if(lp!=NULL && lp[0]){
		//Update cache if needed.
		if(cache==NULL){
			SDL_Color color;
			if(inDialog)
				color=themeTextColorDialog;
			else
				color=themeTextColor;
			
			cache=TTF_RenderUTF8_Blended(fontText,lp,color);
			
			if(width<=0)
				width=cache->w;
		}
		
		//Align the text properly and draw it.
		if(draw){
			if(gravity==GUIGravityCenter)
				gravityX=(width-cache->w)/2;
			else if(gravity==GUIGravityRight)
				gravityX=width-cache->w;
			else
				gravityX=0;
			
			r.y=y+(height - cache->h)/2;
			r.x+=gravityX;
			SDL_BlitSurface(cache,NULL,screen,&r);
		}
	}
}

//////////////GUITextBox///////////////////////////////////////////////////////////////////

void GUITextBox::backspaceChar(){
	//We need to remove a character so first make sure that there is text.
	if(caption.length()>0){
		if(highlightStart==highlightEnd&&highlightStart>0){
			int advance;
			TTF_GlyphMetrics(fontText,caption[highlightEnd-1],NULL,NULL,NULL,NULL,&advance);
			highlightEndX=highlightStartX=highlightEndX-advance;
			
			highlightEnd=highlightStart=highlightEnd-1;
			caption.erase((size_t)highlightEnd,1);
		}else if(highlightStart<highlightEnd){
			caption.erase(highlightStart,highlightEnd-highlightStart);
			highlightEnd=highlightStart;
			highlightEndX=highlightStartX;
		}else{
			caption.erase(highlightEnd,highlightStart-highlightEnd);
			highlightStart=highlightEnd;
			highlightStartX=highlightEndX;
		}
		
		//If there is an event callback then call it.
		if(eventCallback){
			GUIEvent e={eventCallback,name,this,GUIEventChange};
			GUIEventQueue.push_back(e);
		}
	}
}

void GUITextBox::deleteChar(){
	//We need to remove a character so first make sure that there is text.
	if(caption.length()>0){
		if(highlightStart==highlightEnd){
			caption.erase((size_t)highlightEnd,1);
			
			highlightStart=highlightEnd;
			highlightStartX=highlightEndX;
		}else if(highlightStart<highlightEnd){
			caption.erase(highlightStart,highlightEnd-highlightStart);
			
			highlightEnd=highlightStart;
			highlightEndX=highlightStartX;
		}else{
			caption.erase(highlightEnd,highlightStart-highlightEnd);
			
			highlightStart=highlightEnd;
			highlightStartX=highlightEndX;
		}
			
		//If there is an event callback then call it.
		if(eventCallback){
			GUIEvent e={eventCallback,name,this,GUIEventChange};
			GUIEventQueue.push_back(e);
		}
	}
}

void GUITextBox::moveCarrotLeft(){
	if(highlightEnd>0){
		highlightEnd--;
		int advance;
		TTF_GlyphMetrics(fontText,caption.at(highlightEnd),NULL,NULL,NULL,NULL,&advance);
		if(SDL_GetModState() & KMOD_SHIFT){
			highlightEndX-=advance;
		}else{
			highlightStart=highlightEnd;
			highlightStartX=highlightEndX=highlightEndX-advance;
		}
	}else{
		if((SDL_GetModState() & KMOD_SHIFT)==0){
			highlightStart=highlightEnd;
			highlightStartX=highlightEndX;
		}
	}
	tick=15;
}

void GUITextBox::moveCarrotRight(){
	if(highlightEnd<caption.length()){
		int advance;
		TTF_GlyphMetrics(fontText,caption.at(highlightEnd),NULL,NULL,NULL,NULL,&advance);
		if(SDL_GetModState() & KMOD_SHIFT){
			highlightEndX+=advance;
			highlightEnd++;
		}else{
			highlightStartX=highlightEndX=highlightEndX+advance;
			highlightEnd=highlightStart=highlightEnd+1;
		}
	}else{
		if((SDL_GetModState() & KMOD_SHIFT)==0){
			highlightStart=highlightEnd;
			highlightStartX=highlightEndX;
		}
	}
	tick=15;
}

bool GUITextBox::handleEvents(int x,int y,bool enabled,bool visible,bool processed){
	//Boolean if the event is processed.
	bool b=processed;
	
	//The widget is only enabled when its parent are enabled.
	enabled=enabled && this->enabled;
	//The widget is only enabled when its parent are enabled.
	visible=visible && this->visible;
	
	//Get the absolute position.
	x+=left-gravityX;
	y+=top;

	//NOTE: We don't reset the state to have a "focus" effect.
	
	//Only check for events when the object is both enabled and visible.
	if(enabled&&visible){
		//Check if there's a key press and the event hasn't been already processed.
		if(state==2 && event.type==SDL_KEYDOWN && !b){
			//Get the keycode.
			int key=(int)event.key.keysym.unicode;
			
			//Check if the key is supported.
			if(key>=32&&key<=126){
				if(highlightStart==highlightEnd){
					caption.insert((size_t)highlightStart,1,char(key));
					highlightStart++;
					highlightEnd=highlightStart;
				}else if(highlightStart<highlightEnd){
					caption.erase(highlightStart,highlightEnd-highlightStart);
					caption.insert((size_t)highlightStart,1,char(key));
					highlightStart++;
					highlightEnd=highlightStart;
					highlightEndX=highlightStartX;
				}else{
					caption.erase(highlightEnd,highlightStart-highlightEnd);
					caption.insert((size_t)highlightEnd,1,char(key));
					highlightEnd++;
					highlightStart=highlightEnd;
					highlightStartX=highlightEndX;
				}
				int advance;
				TTF_GlyphMetrics(fontText,char(key),NULL,NULL,NULL,NULL,&advance);
				highlightStartX=highlightEndX=highlightStartX+advance;
			
				//If there is an event callback then call it.
				if(eventCallback){
					GUIEvent e={eventCallback,name,this,GUIEventChange};
					GUIEventQueue.push_back(e);
				}
			}else if(event.key.keysym.sym==SDLK_BACKSPACE){
				//Set the key values correctly.
				this->key=SDLK_BACKSPACE;
				keyHoldTime=0;
				keyTime=5;
				
				//Delete one character direct to prevent a lag.
				backspaceChar();
			}else if(event.key.keysym.sym==SDLK_DELETE){
				//Set the key values correctly.
				this->key=SDLK_DELETE;
				keyHoldTime=0;
				keyTime=5;
				
				//Delete one character direct to prevent a lag.
				deleteChar();
			}else if(event.key.keysym.sym==SDLK_RIGHT){
				//Set the key values correctly.
				this->key=SDLK_RIGHT;
				keyHoldTime=0;
				keyTime=5;
				
				//Move directly to prevent a lag.
				moveCarrotRight();
			}else if(event.key.keysym.sym==SDLK_LEFT){
				//Set the key values correctly.
				this->key=SDLK_LEFT;
				keyHoldTime=0;
				keyTime=5;
				
				//Move directly to prevent a lag.
				moveCarrotLeft();
			}
			
			//The event has been processed.
			b=true;
		}else if(state==2 && event.type==SDL_KEYUP && !b){
			//Check if released key is the same as the holded key.
			if(event.key.keysym.sym==key){
				//It is so stop the key.
				key=-1;
			}
		}
		
		//The mouse location (x=i, y=j) and the mouse button (k).
		int i,j,k;
		k=SDL_GetMouseState(&i,&j);
	
		//Check if the mouse is inside the widget.
		if(i>=x&&i<x+width&&j>=y&&j<y+height){
			//We can only increase our state. (nothing->hover->focus).
			if(state!=2){
				state=1;
			}
			
			//Also update the cursor type.
			currentCursor=CURSOR_CARROT;
			
			//Move carrot and highlightning according to mouse input.
			int clickX=i-x-2;
			
			int finalPos=0;
			int finalX=0;
				
			if(cache&&!caption.empty()){
				finalPos=caption.length();
				for(unsigned int i=0;i<caption.length();i++){
					int advance;
					TTF_GlyphMetrics(fontText,caption[i],NULL,NULL,NULL,NULL,&advance);
					finalX+=advance;
					
					if(clickX<finalX-advance/2){
						finalPos=i;
						finalX-=advance;
						break;
					}
				}
			}
			
			if(event.type==SDL_MOUSEBUTTONUP){
				state=2;
				highlightEnd=finalPos;
				highlightEndX=finalX;
			}else if(event.type==SDL_MOUSEBUTTONDOWN){
				state=2;
				highlightStart=highlightEnd=finalPos;
				highlightStartX=highlightEndX=finalX;
			}else if(event.type==SDL_MOUSEMOTION&&(k&SDL_BUTTON(1))){
				state=2;
				highlightEnd=finalPos;
				highlightEndX=finalX;
			}
		}else{
			//The mouse is outside the TextBox.
			//If we don't have focus but only hover we lose it.
			if(state==1){
				state=0;
			}
			
			//If it's a click event outside the textbox then we blur.
			if(event.type==SDL_MOUSEBUTTONUP && event.button.button==SDL_BUTTON_LEFT){
				//Set state to 0.
				state=0;
			}
		}
	}
	
	return b;
}

void GUITextBox::render(int x,int y,bool draw){
	//There's no need drawing the widget when it's invisible.
	if(!visible)
		return;
	
	//Get the absolute x and y location.
	x+=left;
	y+=top;

	//Check if the enabled state changed or the caption, if so we need to clear the (old) cache.
	if(enabled!=cachedEnabled || caption.compare(cachedCaption)!=0 || width<=0){
		//Free the cache.
		SDL_FreeSurface(cache);
		cache=NULL;
		
		//And cache the new values.
		cachedEnabled=enabled;
		cachedCaption=caption;
		
		//Finally resize the widget
		if(autoWidth)
			width=-1;
	}
	
	//FIXME: Logic in the render method since that is update constant.
	if(key!=-1){
		//Increase the key time.
		keyHoldTime++;
		//Make sure the deletionTime isn't to short.
		if(keyHoldTime>=keyTime){
			keyHoldTime=0;
			keyTime--;
			if(keyTime<1)
				keyTime=1;
			
			//Now check the which key it was.
			switch(key){
				case SDLK_BACKSPACE:
					backspaceChar();
					break;
				case SDLK_DELETE:
					deleteChar();
					break;
				case SDLK_LEFT:
					moveCarrotLeft();
					break;
				case SDLK_RIGHT:
					moveCarrotRight();
					break;
			}
		}
	}
	
	if(draw){
		//Default background opacity
		int clr=50;
		//If hovering or focused make background more visible.
		if(state==1)
			clr=128;
		else if (state==2)
			clr=100;
	
		//Draw the box.
		Uint32 color=0xFFFFFF00|clr;
		drawGUIBox(x,y,width,height,screen,color);
	}
	
	//Rectangle used for drawing.
	SDL_Rect r;
	
	//Get the text and make sure it isn't empty.
	const char* lp=caption.c_str();
	if(lp!=NULL && lp[0]){
		if(!cache){
			//Draw the black text.
			SDL_Color black={0,0,0,0};
			cache=TTF_RenderUTF8_Blended(fontText,lp,black);
		}
				
		if(draw){
			//Only draw the carrot and highlight when focus.
			if(state==2){
				//Place the highlighted area.
				r.x=x+4;
				r.y=y+3;
				r.h=height-6;
				
				if(highlightStart<highlightEnd){
					r.x+=highlightStartX;
					r.w=highlightEndX-highlightStartX;
				}else{
					r.x+=highlightEndX;
					r.w=highlightStartX-highlightEndX;
				}
				
				//Draw the area.
				SDL_FillRect(screen,&r,SDL_MapRGB(screen->format,128,128,128));
				
				//Ticking carrot.
				if(tick<16){
					//Show carrot: 15->0.
					r.x=x+highlightEndX+2;
					r.y=y+3;
					r.h=height-6;
					r.w=2;
					SDL_FillRect(screen,&r,SDL_MapRGB(screen->format,0,0,0));
					
					//Reset: 32 or count down.
					if(tick<=0)
						tick=32;
					else
						tick--;
				}else{
					//Hide carrot: 32->16.
					tick--;
				}
			}
			
			//Calculate the location, center it vertically.
			r.x=x+4;
			r.y=y+(height - cache->h)/2;
		
			//Draw the text.
			SDL_Rect tmp={0,0,width-2,25};
			SDL_BlitSurface(cache,&tmp,screen,&r);
		}
	}else{
		//Only draw the carrot when focus.
		if(state==2&&draw){
			r.x=x+4;
			r.y=y+4;
			r.w=2;
			r.h=height-8;
			SDL_FillRect(screen,&r,0);
		}
	}
}

//////////////GUIFrame///////////////////////////////////////////////////////////////////

bool GUIFrame::handleEvents(int x,int y,bool enabled,bool visible,bool processed){
	//Boolean if the event is processed.
	bool b=processed;
	
	//The widget is only enabled when its parent are enabled.
	enabled=enabled && this->enabled;
	//The widget is only enabled when its parent are enabled.
	visible=visible && this->visible;
	
	//Get the absolute position.
	x+=left;
	y+=top;
	
	//Also let the children handle their events.
	for(unsigned int i=0;i<childControls.size();i++){
		bool b1=childControls[i]->handleEvents(x,y,enabled,visible,b);
		
		//The event is processed when either our or the childs is true (or both).
		b=b||b1;
	}
	return b;
}

void GUIFrame::render(int x,int y,bool draw){
	//There's no need drawing this widget when it's invisible.
	if(!visible)
		return;
	
	//Get the absolute x and y location.
	x+=left;
	y+=top;
	
	//Check if the enabled state changed or the caption, if so we need to clear the (old) cache.
	if(enabled!=cachedEnabled || caption.compare(cachedCaption)!=0 || width<=0){
		//Free the cache.
		SDL_FreeSurface(cache);
		cache=NULL;
		
		//And cache the new values.
		cachedEnabled=enabled;
		cachedCaption=caption;
		
		//Finally resize the widget.
		if(autoWidth)
			width=-1;
	}
	
	//Draw fill and borders.
	if(draw){
		Uint32 color=0xDDDDDDFF;
		drawGUIBox(x,y,width,height,screen,color);
	}
	
	//Get the title text and make sure it isn't empty.
	const char* lp=caption.c_str();
	if(lp!=NULL && lp[0]){
		//Update cache if needed.
		if(!cache)
			cache=TTF_RenderUTF8_Blended(fontGUI,lp,themeTextColorDialog);
		
		//Draw the text.
		if(draw)
			applySurface(x+(width-cache->w)/2,y+6-GUI_FONT_RAISE,cache,screen,NULL);
	}
	
	//We now need to draw all the children.
	for(unsigned int i=0;i<childControls.size();i++){
		childControls[i]->render(x,y,draw);
	}
}

//////////////GUIImage///////////////////////////////////////////////////////////////////

GUIImage::~GUIImage(){
	//Check if the surface is managed, if so free it.
	if(managed)
		SDL_FreeSurface(image);
}

bool GUIImage::handleEvents(int x,int y,bool enabled,bool visible,bool processed){
	return processed;
}

void GUIImage::fitToImage(){
	//Increase or decrease the width and height to fully show the image.
	if(clip.w!=0)
		width=clip.w;
	else
		width=image->w;
	if(clip.h!=0)
		height=clip.h;
	else
		height=image->h;
}

void GUIImage::render(int x,int y,bool draw){
	//There's no need drawing the widget when it's invisible.
	if(!visible)
		return;
	
	//Get the absolute x and y location.
	x+=left;
	y+=top;

	//Create a clip rectangle.
	SDL_Rect r;
	//The width and height are capped by the GUIImage itself.
	r=clip;
	if(r.w>width || r.w==0)
		r.w=width;
	if(r.h>height || r.h==0)
		r.h=height;
	
	//Make sure the image isn't null.
	if(image)
		applySurface(x,y,image,screen,&r);
}
