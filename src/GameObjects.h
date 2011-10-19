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
#ifndef GAME_OBJECTS_H
#define GAME_OBJECTS_H

#include <SDL/SDL.h>
#include <string>
#include <vector>
#include <utility>
#include <map>
#include "Globals.h"

class Game;
class Player;

enum GameObjectEventType{
	GameObjectEvent_PlayerWalkOn=1,
	GameObjectEvent_PlayerIsOn,
	GameObjectEvent_PlayerLeave, //unimplemented
	GameObjectEvent_OnToggle=0x10000,
	GameObjectEvent_OnSwitchOn=0x10001,
	GameObjectEvent_OnSwitchOff=0x10002,
};

enum GameObjectPropertyType{
	GameObjectProperty_PlayerCanWalkOn=1,
	GameObjectProperty_IsSpikes,
	GameObjectProperty_Flags,
};

enum GameObjectBoxType{
	BoxType_Current=0, //current position
	BoxType_Base, //base position (used in level editor)
	BoxType_Previous, //last position
	BoxType_Delta, //block movement since last position
	BoxType_Velocity, //velocity if player is standing on it
};

class GameObject{
protected:
	SDL_Rect box;
	
public:

	int type;
	Game *objParent;

	GameObject(Game *objParent);
	~GameObject();

	virtual SDL_Rect getBox(int boxType=0);

	virtual void show() = 0;
	virtual void saveState();
	virtual void loadState();
	virtual void reset();
	//
	virtual void playAnimation(int flags);
	virtual void onEvent(int eventType);
	virtual int queryProperties(int propertyType,Player* obj);
	//
	virtual void getEditorData(std::vector<std::pair<std::string,std::string> >& obj);
	virtual void setEditorData(std::map<std::string,std::string>& obj);
	virtual void move();
};

#include "Block.h"

#endif