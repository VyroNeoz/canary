/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2022 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

#include "declarations.hpp"
#include "lib/di/soft_singleton.hpp"
#include "lua/scripts/luascript.hpp"
#include "creatures/players/player.hpp"

class Game;

class Charm {
public:
	Charm() = default;
	Charm(std::string initname, charmRune_t initcharmRune_t, std::string initdescription, charm_t inittype, uint16_t initpoints, int32_t initbinary) :
		name(initname), id(initcharmRune_t), description(initdescription), type(inittype), points(initpoints), binary(initbinary) { }
	virtual ~Charm() = default;

	std::string name;
	std::string cancelMsg;
	std::string logMsg;
	std::string description;

	charm_t type;
	charmRune_t id = CHARM_NONE;
	CombatType_t dmgtype = COMBAT_NONE;
	uint16_t effect = CONST_ME_NONE;

	SoundEffect_t soundImpactEffect = SoundEffect_t::SILENCE;
	SoundEffect_t soundCastEffect = SoundEffect_t::SILENCE;

	uint16_t percent = 0;
	int8_t chance = 0;
	uint16_t points = 0;
	int32_t binary = 0;
};

class IOBestiary {
public:
	IOBestiary() = default;

	// non-copyable
	IOBestiary(const IOBestiary &) = delete;
	void operator=(const IOBestiary &) = delete;

	static IOBestiary &getInstance() {
		return inject<IOBestiary>();
	}

	std::shared_ptr<Charm> getBestiaryCharm(charmRune_t activeCharm, bool force = false) const;
	void addBestiaryKill(Player* player, const std::shared_ptr<MonsterType> &mtype, uint32_t amount = 1);
	bool parseCharmCombat(const std::shared_ptr<Charm> &charm, Player* player, Creature* target, int32_t realDamage, bool dueToPotion = false, bool checkArmor = false);
	void addCharmPoints(Player* player, uint16_t amount, bool negative = false);
	void sendBuyCharmRune(Player* player, charmRune_t runeID, uint8_t action, uint16_t raceid);
	void setCharmRuneCreature(Player* player, const std::shared_ptr<Charm> &charm, uint16_t raceid);
	void resetCharmRuneCreature(Player* player, const std::shared_ptr<Charm> &charm);

	int8_t calculateDifficult(uint32_t chance) const;
	uint8_t getKillStatus(const std::shared_ptr<MonsterType> &mtype, uint32_t killAmount) const;

	uint16_t getBestiaryRaceUnlocked(Player* player, BestiaryType_t race) const;

	int32_t bitToggle(int32_t input, const std::shared_ptr<Charm> &charm, bool on) const;

	bool hasCharmUnlockedRuneBit(const std::shared_ptr<Charm> &charm, int32_t input) const;

	std::list<charmRune_t> getCharmUsedRuneBitAll(Player* player);
	phmap::parallel_flat_hash_set<uint16_t> getBestiaryFinished(Player* player) const;

	charmRune_t getCharmFromTarget(Player* player, const std::shared_ptr<MonsterType> &mtype);

	std::map<uint16_t, uint32_t> getBestiaryKillCountByMonsterIDs(Player* player, std::map<uint16_t, std::string> mtype_list) const;
	std::map<uint8_t, int16_t> getMonsterElements(const std::shared_ptr<MonsterType> &mtype) const;
	std::map<uint16_t, std::string> findRaceByName(const std::string &race, bool Onlystring = true, BestiaryType_t raceNumber = BESTY_RACE_NONE) const;

private:
	static SoftSingleton instanceTracker;
	SoftSingletonGuard guard { instanceTracker };
};

constexpr auto g_iobestiary = IOBestiary::getInstance;
