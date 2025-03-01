/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2022 OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "pch.hpp"

#include "io/iologindata.hpp"
#include "io/functions/iologindata_load_player.hpp"
#include "io/functions/iologindata_save_player.hpp"
#include "game/game.hpp"
#include "creatures/monsters/monster.hpp"
#include "creatures/players/wheel/player_wheel.hpp"
#include "io/ioprey.hpp"
#include "security/argon.hpp"

bool IOLoginData::authenticateAccountPassword(const std::string &accountIdentifier, const std::string &password, account::Account* account) {
	if (account::ERROR_NO != account->LoadAccountDB(accountIdentifier)) {
		g_logger().error("{} {} doesn't match any account.", account->getProtocolCompat() ? "Username" : "Email", accountIdentifier);
		return false;
	}

	std::string accountPassword;
	account->GetPassword(&accountPassword);

	Argon2 argon2;
	if (!argon2.argon(password.c_str(), accountPassword)) {
		if (transformToSHA1(password) != accountPassword) {
			g_logger().error("Password '{}' doesn't match any account", accountPassword);
			return false;
		}
	}

	return true;
}

bool IOLoginData::authenticateAccountSession(const std::string &sessionId, account::Account* account) {
	Database &db = Database::getInstance();
	std::ostringstream query;
	query << "SELECT `account_id`, `expires` FROM `account_sessions` WHERE `id` = " << db.escapeString(transformToSHA1(sessionId));
	DBResult_ptr result = Database::getInstance().storeQuery(query.str());
	if (!result) {
		g_logger().error("Session id {} not found in the database", sessionId);
		return false;
	}
	uint32_t expires = result->getNumber<uint32_t>("expires");
	if (expires < getTimeNow()) {
		g_logger().error("Session id {} found, but it is expired", sessionId);
		return false;
	}
	uint32_t accountId = result->getNumber<uint32_t>("account_id");
	if (account::ERROR_NO != account->LoadAccountDB(accountId)) {
		g_logger().error("Session id {} found account id {}, but it doesn't match any account.", sessionId, accountId);
		return false;
	}

	return true;
}

bool IOLoginData::gameWorldAuthentication(const std::string &accountIdentifier, const std::string &sessionOrPassword, std::string &characterName, uint32_t* accountId, bool oldProtocol) {
	account::Account account;
	account.setProtocolCompat(oldProtocol);
	std::string authType = g_configManager().getString(AUTH_TYPE);

	if (authType == "session") {
		if (!IOLoginData::authenticateAccountSession(sessionOrPassword, &account)) {
			return false;
		}
	} else { // authType == "password"
		if (!IOLoginData::authenticateAccountPassword(accountIdentifier, sessionOrPassword, &account)) {
			return false;
		}
	}

	account::Player player;
	if (account::ERROR_NO != account.GetAccountPlayer(&player, characterName)) {
		g_logger().error("Player not found or deleted for account.");
		return false;
	}

	account.GetID(accountId);

	return true;
}

account::AccountType IOLoginData::getAccountType(uint32_t accountId) {
	std::ostringstream query;
	query << "SELECT `type` FROM `accounts` WHERE `id` = " << accountId;
	DBResult_ptr result = Database::getInstance().storeQuery(query.str());
	if (!result) {
		return account::ACCOUNT_TYPE_NORMAL;
	}
	return static_cast<account::AccountType>(result->getNumber<uint16_t>("type"));
}

void IOLoginData::setAccountType(uint32_t accountId, account::AccountType accountType) {
	std::ostringstream query;
	query << "UPDATE `accounts` SET `type` = " << static_cast<uint16_t>(accountType) << " WHERE `id` = " << accountId;
	Database::getInstance().executeQuery(query.str());
}

void IOLoginData::updateOnlineStatus(uint32_t guid, bool login) {
	static phmap::flat_hash_map<uint32_t, bool> updateOnline;
	if (login && updateOnline.find(guid) != updateOnline.end() || guid <= 0) {
		return;
	}

	std::ostringstream query;
	if (login) {
		query << "INSERT INTO `players_online` VALUES (" << guid << ')';
		updateOnline[guid] = true;
	} else {
		query << "DELETE FROM `players_online` WHERE `player_id` = " << guid;
		updateOnline.erase(guid);
	}
	Database::getInstance().executeQuery(query.str());
}

// The boolean "disable" will desactivate the loading of information that is not relevant to the preload, for example, forge, bosstiary, etc. None of this we need to access if the player is offline
bool IOLoginData::loadPlayerById(Player* player, uint32_t id, bool disable /* = true*/) {
	Database &db = Database::getInstance();
	std::ostringstream query;
	query << "SELECT * FROM `players` WHERE `id` = " << id;
	return loadPlayer(player, db.storeQuery(query.str()), disable);
}

bool IOLoginData::loadPlayerByName(Player* player, const std::string &name, bool disable /* = true*/) {
	Database &db = Database::getInstance();
	std::ostringstream query;
	query << "SELECT * FROM `players` WHERE `name` = " << db.escapeString(name);
	return loadPlayer(player, db.storeQuery(query.str()), disable);
}

bool IOLoginData::loadPlayer(Player* player, DBResult_ptr result, bool disable /* = false*/) {
	if (!result || !player) {
		g_logger().warn("[IOLoginData::loadPlayer] - Player or Resultnullptr: {}", __FUNCTION__);
		return false;
	}

	try {
		// First
		IOLoginDataLoad::loadPlayerFirst(player, result);

		// Experience load
		IOLoginDataLoad::loadPlayerExperience(player, result);

		// Blessings load
		IOLoginDataLoad::loadPlayerBlessings(player, result);

		// load conditions
		IOLoginDataLoad::loadPlayerConditions(player, result);

		// load default outfit
		IOLoginDataLoad::loadPlayerDefaultOutfit(player, result);

		// skull system load
		IOLoginDataLoad::loadPlayerSkullSystem(player, result);

		// skill load
		IOLoginDataLoad::loadPlayerSkill(player, result);

		// kills load
		IOLoginDataLoad::loadPlayerKills(player, result);

		// guild load
		IOLoginDataLoad::loadPlayerGuild(player, result);

		// stash load items
		IOLoginDataLoad::loadPlayerStashItems(player, result);

		// bestiary charms
		IOLoginDataLoad::loadPlayerBestiaryCharms(player, result);

		// load inventory items
		IOLoginDataLoad::loadPlayerInventoryItems(player, result);

		// store Inbox
		IOLoginDataLoad::loadPlayerStoreInbox(player);

		// load depot items
		IOLoginDataLoad::loadPlayerDepotItems(player, result);

		// load reward items
		IOLoginDataLoad::loadRewardItems(player);

		// load inbox items
		IOLoginDataLoad::loadPlayerInboxItems(player, result);

		// load storage map
		IOLoginDataLoad::loadPlayerStorageMap(player, result);

		// load vip
		IOLoginDataLoad::loadPlayerVip(player, result);

		// load prey class
		IOLoginDataLoad::loadPlayerPreyClass(player, result);

		// Load task hunting class
		IOLoginDataLoad::loadPlayerTaskHuntingClass(player, result);

		// load forge history
		IOLoginDataLoad::loadPlayerForgeHistory(player, result);

		// load bosstiary
		IOLoginDataLoad::loadPlayerBosstiary(player, result);

		IOLoginDataLoad::loadPlayerInitializeSystem(player);
		IOLoginDataLoad::loadPlayerUpdateSystem(player);

		return true;
	} catch (const std::system_error &error) {
		g_logger().warn("[{}] Error while load player: {}", __FUNCTION__, error.what());
		return false;
	} catch (const std::exception &e) {
		g_logger().warn("[{}] Error while load player: {}", __FUNCTION__, e.what());
		return false;
	}
}

bool IOLoginData::savePlayer(Player* player) {
	bool success = DBTransaction::executeWithinTransaction([player]() {
		return savePlayerGuard(player);
	});

	if (!success) {
		g_logger().error("[{}] Error occurred saving player", __FUNCTION__);
	}

	return success;
}

bool IOLoginData::savePlayerGuard(Player* player) {
	if (!player) {
		throw DatabaseException("Player nullptr in function: " + std::string(__FUNCTION__));
	}

	if (!IOLoginDataSave::savePlayerFirst(player)) {
		throw DatabaseException("[" + std::string(__FUNCTION__) + "] - Failed to save player first: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerStash(player)) {
		throw DatabaseException("[IOLoginDataSave::savePlayerFirst] - Failed to save player stash: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerSpells(player)) {
		throw DatabaseException("[IOLoginDataSave::savePlayerSpells] - Failed to save player spells: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerKills(player)) {
		throw DatabaseException("IOLoginDataSave::savePlayerKills] - Failed to save player kills: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerBestiarySystem(player)) {
		throw DatabaseException("[IOLoginDataSave::savePlayerBestiarySystem] - Failed to save player bestiary system: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerItem(player)) {
		throw DatabaseException("[IOLoginDataSave::savePlayerItem] - Failed to save player item: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerDepotItems(player)) {
		throw DatabaseException("[IOLoginDataSave::savePlayerDepotItems] - Failed to save player depot items: " + player->getName());
	}

	if (!IOLoginDataSave::saveRewardItems(player)) {
		throw DatabaseException("[IOLoginDataSave::saveRewardItems] - Failed to save player reward items: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerInbox(player)) {
		throw DatabaseException("[IOLoginDataSave::savePlayerInbox] - Failed to save player inbox: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerPreyClass(player)) {
		throw DatabaseException("[IOLoginDataSave::savePlayerPreyClass] - Failed to save player prey class: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerTaskHuntingClass(player)) {
		throw DatabaseException("[IOLoginDataSave::savePlayerTaskHuntingClass] - Failed to save player task hunting class: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerForgeHistory(player)) {
		throw DatabaseException("[IOLoginDataSave::savePlayerForgeHistory] - Failed to save player forge history: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerBosstiary(player)) {
		throw DatabaseException("[IOLoginDataSave::savePlayerBosstiary] - Failed to save player bosstiary: " + player->getName());
	}

	if (!player->wheel()->saveDBPlayerSlotPointsOnLogout()) {
		throw DatabaseException("[PlayerWheel::saveDBPlayerSlotPointsOnLogout] - Failed to save player wheel info: " + player->getName());
	}

	if (!IOLoginDataSave::savePlayerStorage(player)) {
		throw DatabaseException("[IOLoginDataSave::savePlayerStorage] - Failed to save player storage: " + player->getName());
	}

	return true;
}

std::string IOLoginData::getNameByGuid(uint32_t guid) {
	std::ostringstream query;
	query << "SELECT `name` FROM `players` WHERE `id` = " << guid;
	DBResult_ptr result = Database::getInstance().storeQuery(query.str());
	if (!result) {
		return std::string();
	}
	return result->getString("name");
}

uint32_t IOLoginData::getGuidByName(const std::string &name) {
	Database &db = Database::getInstance();

	std::ostringstream query;
	query << "SELECT `id` FROM `players` WHERE `name` = " << db.escapeString(name);
	DBResult_ptr result = db.storeQuery(query.str());
	if (!result) {
		return 0;
	}
	return result->getNumber<uint32_t>("id");
}

bool IOLoginData::getGuidByNameEx(uint32_t &guid, bool &specialVip, std::string &name) {
	Database &db = Database::getInstance();

	std::ostringstream query;
	query << "SELECT `name`, `id`, `group_id`, `account_id` FROM `players` WHERE `name` = " << db.escapeString(name);
	DBResult_ptr result = db.storeQuery(query.str());
	if (!result) {
		return false;
	}

	name = result->getString("name");
	guid = result->getNumber<uint32_t>("id");
	if (auto group = g_game().groups.getGroup(result->getNumber<uint16_t>("group_id"))) {
		specialVip = group->flags[Groups::getFlagNumber(PlayerFlags_t::SpecialVIP)];
	} else {
		specialVip = false;
	}
	return true;
}

bool IOLoginData::formatPlayerName(std::string &name) {
	Database &db = Database::getInstance();

	std::ostringstream query;
	query << "SELECT `name` FROM `players` WHERE `name` = " << db.escapeString(name);

	DBResult_ptr result = db.storeQuery(query.str());
	if (!result) {
		return false;
	}

	name = result->getString("name");
	return true;
}

void IOLoginData::increaseBankBalance(uint32_t guid, uint64_t bankBalance) {
	std::ostringstream query;
	query << "UPDATE `players` SET `balance` = `balance` + " << bankBalance << " WHERE `id` = " << guid;
	Database::getInstance().executeQuery(query.str());
}

bool IOLoginData::hasBiddedOnHouse(uint32_t guid) {
	Database &db = Database::getInstance();

	std::ostringstream query;
	query << "SELECT `id` FROM `houses` WHERE `highest_bidder` = " << guid << " LIMIT 1";
	return db.storeQuery(query.str()).get() != nullptr;
}

std::forward_list<VIPEntry> IOLoginData::getVIPEntries(uint32_t accountId) {
	std::forward_list<VIPEntry> entries;

	std::ostringstream query;
	query << "SELECT `player_id`, (SELECT `name` FROM `players` WHERE `id` = `player_id`) AS `name`, `description`, `icon`, `notify` FROM `account_viplist` WHERE `account_id` = " << accountId;

	DBResult_ptr result = Database::getInstance().storeQuery(query.str());
	if (result) {
		do {
			entries.emplace_front(
				result->getNumber<uint32_t>("player_id"),
				result->getString("name"),
				result->getString("description"),
				result->getNumber<uint32_t>("icon"),
				result->getNumber<uint16_t>("notify") != 0
			);
		} while (result->next());
	}
	return entries;
}

void IOLoginData::addVIPEntry(uint32_t accountId, uint32_t guid, const std::string &description, uint32_t icon, bool notify) {
	Database &db = Database::getInstance();

	std::ostringstream query;
	query << "INSERT INTO `account_viplist` (`account_id`, `player_id`, `description`, `icon`, `notify`) VALUES (" << accountId << ',' << guid << ',' << db.escapeString(description) << ',' << icon << ',' << notify << ')';
	db.executeQuery(query.str());
}

void IOLoginData::editVIPEntry(uint32_t accountId, uint32_t guid, const std::string &description, uint32_t icon, bool notify) {
	Database &db = Database::getInstance();

	std::ostringstream query;
	query << "UPDATE `account_viplist` SET `description` = " << db.escapeString(description) << ", `icon` = " << icon << ", `notify` = " << notify << " WHERE `account_id` = " << accountId << " AND `player_id` = " << guid;
	db.executeQuery(query.str());
}

void IOLoginData::removeVIPEntry(uint32_t accountId, uint32_t guid) {
	std::ostringstream query;
	query << "DELETE FROM `account_viplist` WHERE `account_id` = " << accountId << " AND `player_id` = " << guid;
	Database::getInstance().executeQuery(query.str());
}

void IOLoginData::addPremiumDays(Player* player, uint32_t addDays) {
	std::ostringstream query;
	time_t lastDay = player->getPremiumLastDay();
	query << "UPDATE `accounts` SET"
		  << "`premdays` = `premdays` + " << addDays
		  << ", `premdays_purchased` = `premdays_purchased` + " << addDays
		  << ", `lastday` = " << (((lastDay == 0 || lastDay < getTimeNow()) ? getTimeNow() : lastDay) + (addDays * 86400))
		  << " WHERE `id` = " << player->getAccount();

	Database::getInstance().executeQuery(query.str());
}

void IOLoginData::removePremiumDays(Player* player, uint32_t removeDays) {
	std::ostringstream query;
	uint32_t days = removeDays > player->premiumDays ? player->premiumDays : removeDays;
	query << "UPDATE `accounts` SET"
		  << "`premdays` = `premdays` - " << days
		  << ", `lastday` = " << (player->getPremiumLastDay() - (days * 86400))
		  << " WHERE `id` = " << player->getAccount();
	Database::getInstance().executeQuery(query.str());
}
