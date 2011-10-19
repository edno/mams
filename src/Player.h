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
#ifndef PLAYER_H
#define PLAYER_H

#include "ThemeManager.h"
#include <vector>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

class GameObject;
class Game;

const int PlayerButtonRight=0x01;
const int PlayerButtonLeft=0x02;
const int PlayerButtonJump=0x04;
const int PlayerButtonDown=0x08;

class Player{
protected:

	std::vector<int> player_button;

private:
	std::vector<SDL_Rect> line;

	bool b_shadow_call;
	bool b_record;
	//new
	SDL_Rect box_saved;
	bool b_inAir_saved;
	bool b_jump_saved;
	bool b_on_ground_saved;
	bool b_can_move_saved;
	bool b_holding_other_saved;
	int i_xVel_saved, i_yVel_saved;
	//end
	
protected:
	SDL_Rect box;

	int i_xVel, i_yVel;
	int i_xVel_base, i_yVel_base;

	Mix_Chunk* c_jump;
	Mix_Chunk* c_hit;
	Mix_Chunk* c_save;
	Mix_Chunk* c_swap;
	Mix_Chunk* c_toggle;

	bool b_inAir;
	bool b_jump;
	bool b_on_ground;
	bool b_can_move;
	bool b_dead;


	int i_frame;
	int i_animation;
	int i_direction;
	int i_state;
	int i_jump_time;
	bool b_shadow;

	friend class Game;
	Game* m_objParent;

	//new
	bool bDownKeyPressed;
	GameObject *m_objCurrentStand; //always be valid pointer
	GameObject *m_objLastStand; //warning: weak reference only
	GameObject *m_objLastTeleport; //warning: weak reference only
	//end

public:

	int i_fx, i_fy;
	ThemeCharacterInstance Appearance;
	bool b_holding_other;

	Player(Game* objParent);
	~Player();

	void setPosition(int x,int y);

	void handleInput(class Shadow * shadow);
	void move(std::vector<GameObject*> &LevelObjects);
	void jump();
	void show();
	void shadowSetState();
	virtual void stateReset();
	void otherCheck(class Player* other);
	void setMyCamera();
	void reset();
	SDL_Rect getBox();

	void shadowGiveState(class Shadow* shadow);
	//new
	virtual void saveState();
	virtual void loadState();
	virtual bool canSaveState();
	virtual bool canLoadState();
	void swapState(Player * other);
	inline bool isShadow(){return b_shadow;}
	void die();
	//end
};

#endif
