// MIT License
// 
// Copyright(c) 2020 Arthur Bacon and Kevin Dill
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Controller_AI_RayNamar.h"

#include "Constants.h"
#include "EntityStats.h"
#include "iPlayer.h"
#include "Vec2.h"
#include <ctime>

static const Vec2 ksGiantPosLeft(LEFT_BRIDGE_CENTER_X, RIVER_TOP_Y - 0.5f);
static const Vec2 ksGiantPosRight(RIGHT_BRIDGE_CENTER_X, RIVER_TOP_Y - 0.5f);

static const Vec2 ksArcherPosLeft(LEFT_BRIDGE_CENTER_X - 2.0f, 0.f);
static const Vec2 ksArcherPosRight(RIGHT_BRIDGE_CENTER_X + 2.0f, 0.f);

static const Vec2 ksSwordPosLeft(LEFT_BRIDGE_CENTER_X, 0.f);
static const Vec2 ksSwordPosRight(RIGHT_BRIDGE_CENTER_X, 0.f);

// the cheapest available unit (defaults to archer)
static iEntityStats::MobType minElixirMob = iEntityStats::Archer;

void Controller_AI_RayNamar::tick(float deltaTSec)
{
    assert(m_pPlayer);

	bool isNorth = m_pPlayer->isNorth();

	if (!minCalced) { 
		Controller_AI_RayNamar::findMinElixirMob();
		minCalced = true;
	}

	float cur_elixer = m_pPlayer->getElixir();

	// calculate the enemy pressure every tick
	calcEnemyPressure();

	// if neither side is being more overwhelmed, remain balanced
	if (-3000 < imbalance < 3000) {
		curr_personality = Personality::balanced;
	}
	// otherwise, focus on the worse side
	else {
		curr_personality = Personality::focused;
		// make sure to focus on the lane that is being heavily assaulted
		atkLane = (imbalance > 0) ? true : false;
	}

	// use the queued move if we have enough elixer. then choose another move
	if (queuedMove == Combos::classic && cur_elixer >= 9) {
		classicCombo(isNorth, atkLane);
		std::cout << "Classic Combo!" << std::endl;
		chooseNewMoveAndFlip();
	}
	else if (queuedMove == Combos::sword && cur_elixer >= 6) {
		swordCombo(isNorth, atkLane);
		std::cout << "Special Sword Strike!" << std::endl;
		chooseNewMoveAndFlip();
	}
	else if (queuedMove == Combos::range && cur_elixer >= 4) {
		rangeCombo(isNorth, atkLane);
		std::cout << "Archer Assault!" << std::endl;
		chooseNewMoveAndFlip();
	}
	else if (queuedMove == Combos::balance && cur_elixer >= 5) {
		balanceCombo(isNorth, atkLane);
		std::cout << "Balanced Battalion!" << std::endl;
		chooseNewMoveAndFlip();
	}
	else if (queuedMove == Combos::loneGiant && cur_elixer >= 5) {
		loneGiantCombo(isNorth, atkLane);
		std::cout << "Lonely Giant!" << std::endl;
		chooseNewMoveAndFlip();
	}
	else if (queuedMove == Combos::rushCheap && cur_elixer >= 5) {
		rushCheapUnits(isNorth, atkLane);
		std::cout << "Quickly!" << std::endl;
		chooseNewMoveAndFlip();
	}

	// if we are getting severe pressure, spam cheap units
	if (curr_left_pressure == Personality::focused && (abs(imbalance) > 4000)) {
		queuedMove = rushCheap;
		std::cout << "Under Heavy Assault!" << std::endl;
	}

}

// pressure based on the dps potential of a side, as well as health and range
void Controller_AI_RayNamar::calcEnemyPressure() {
	float potentialLeftDamage = 0.f;
	float potentialRightDamage = 0.f;

	for (int i = 0; i < m_pPlayer->getNumOpponentMobs(); i++) {
		iPlayer::EntityData mob = m_pPlayer->getOpponentMob(i);
		//true = left, false = right
		bool side = mob.m_Position.x < GAME_GRID_WIDTH / 2.0f;

		int mob_damage = mob.m_Stats.getDamage();
		float mob_atk_speed = mob.m_Stats.getAttackTime();

		float mob_dps = mob_damage / mob_atk_speed; // damage over time = dps
		//add health to differentiate similar dps' and give a more detailed diagnostic
		float totalPotential = mob_dps + mob.m_Stats.getMaxHealth();

		//ranged is dangerous, gets a multiplier to add higher potential
		if (mob.m_Stats.getAttackRange() > 1) {
			totalPotential *= 1.5;
		}

		if (side) {
			potentialLeftDamage += totalPotential;
		}
		else {
			potentialRightDamage += totalPotential;
		}
	}
	this->curr_left_pressure = potentialLeftDamage;
	this->curr_right_pressure = potentialRightDamage;
	// calculates which side is currently being more threatened
	this->imbalance = curr_left_pressure - curr_right_pressure;

	//sword DPS: 139
	//archer DPS: 143
	//giant DPS: 141

	//sword potential: 1591
	//archer potential: 538
	//giant potential: 3416
}

// looks through all available mobs and saves the cheapest one. Used for rush tactics
void Controller_AI_RayNamar::findMinElixirMob() {
	auto allMobs = m_pPlayer->GetAvailableMobTypes();
	for (iEntityStats::MobType mob : allMobs) {
		const iEntityStats& stats = iEntityStats::getStats(mob);
		if (stats.getElixirCost() < minElixirCost) {
			minElixirCost = stats.getElixirCost();
			minElixirMob = mob;
		}
	}
}

// randomly picks a new combo to queue
void Controller_AI_RayNamar::chooseNewMoveAndFlip() {
	srand((unsigned)time(0));
	int newMove = (rand() % 5) + 1;
	queuedMove = Combos(newMove);

	//if we are focused, we should not be alternating. otherwise, alternate which path we are sending
	if (curr_personality == Personality::balanced) {
		atkLane = !atkLane;
	}
}

// a giant and two archers
void Controller_AI_RayNamar::classicCombo(bool isNorth, bool aggroSide) {
	Vec2 giantPos_Game;
	Vec2 archerPos_Game;
	if (aggroSide) {
		giantPos_Game = ksGiantPosLeft.Player2Game(isNorth);
		archerPos_Game = ksArcherPosLeft.Player2Game(isNorth);
	}
	else {
		giantPos_Game = ksGiantPosRight.Player2Game(isNorth);
		archerPos_Game = ksArcherPosRight.Player2Game(isNorth);
	}

	// Create two archers and a giant
	m_pPlayer->placeMob(iEntityStats::Giant, giantPos_Game);
	m_pPlayer->placeMob(iEntityStats::Archer, archerPos_Game);
	m_pPlayer->placeMob(iEntityStats::Archer, archerPos_Game);
}

// all swords
void Controller_AI_RayNamar::swordCombo(bool isNorth, bool aggroSide) {
	Vec2 swordPos_Game;
	if (aggroSide) {
		swordPos_Game = ksSwordPosLeft.Player2Game(isNorth);
	}
	else {
		swordPos_Game = ksSwordPosRight.Player2Game(isNorth);
	}
	// create two swordsmen
	m_pPlayer->placeMob(iEntityStats::Swordsman, swordPos_Game);
	m_pPlayer->placeMob(iEntityStats::Swordsman, swordPos_Game);
}

// ranged and melee
void Controller_AI_RayNamar::balanceCombo(bool isNorth, bool aggroSide) {
	Vec2 swordPos_Game;
	Vec2 archerPos_Game;
	if (aggroSide) {
		swordPos_Game = ksSwordPosLeft.Player2Game(isNorth);
		archerPos_Game = ksArcherPosLeft.Player2Game(isNorth);
	}
	else {
		swordPos_Game = ksSwordPosRight.Player2Game(isNorth);
		archerPos_Game = ksArcherPosRight.Player2Game(isNorth);
	}
	// create a swordsman and an archer
	m_pPlayer->placeMob(iEntityStats::Swordsman, swordPos_Game);
	m_pPlayer->placeMob(iEntityStats::Archer, archerPos_Game);
}

// all ranged
void Controller_AI_RayNamar::rangeCombo(bool isNorth, bool aggroSide) {
	Vec2 archerPos_Game;
	if (aggroSide) {
		archerPos_Game = ksArcherPosLeft.Player2Game(isNorth);
	}
	else {
		archerPos_Game = ksArcherPosRight.Player2Game(isNorth);
	}

	// create two archers
	m_pPlayer->placeMob(iEntityStats::Archer, archerPos_Game);
	m_pPlayer->placeMob(iEntityStats::Archer, archerPos_Game);
}

// solo giant
void Controller_AI_RayNamar::loneGiantCombo(bool isNorth, bool aggroSide) { 
	Vec2 giantPos_Game;
	if (aggroSide) {
		giantPos_Game = ksGiantPosLeft.Player2Game(isNorth);
	}
	else {
		giantPos_Game = ksGiantPosRight.Player2Game(isNorth);
	}

	// Create a lone giant
	m_pPlayer->placeMob(iEntityStats::Giant, giantPos_Game);
}

// send out cheapest unit (meant to be called repeatedly)
void Controller_AI_RayNamar::rushCheapUnits(bool isNorth, bool aggroSide) {
	Vec2 unitPos_Game;
	if (aggroSide) {
		unitPos_Game = ksSwordPosLeft.Player2Game(isNorth);
	}
	else {
		unitPos_Game = ksSwordPosRight.Player2Game(isNorth);
	}

	// Create a cheap unit
	m_pPlayer->placeMob(minElixirMob, unitPos_Game);
}