local positionByDay = {
	[1] = {position = Position(32328, 31782, 6), city = "Carlin"}, -- Sunday
	[2] = {position = Position(32207, 31155, 7), city = "Svarground"}, -- Monday
	[3] = {position = Position(32300, 32837, 7), city = "Liberty Bay"}, -- Tuesday
	[4] = {position = Position(32577, 32753, 7), city = "Port Hope"}, -- Wednesday
	[5] = {position = Position(33066, 32879, 6), city = "Ankrahmun"}, -- Thursday
	[6] = {position = Position(33235, 32483, 7), city = "Darashia"}, -- Friday
	[7] = {position = Position(33166, 31810, 6), city = "Edron"}  -- Saturday
}

local function rashidwebhook(message) -- New local function that runs on delay to send webhook message.
	Webhook.sendMessage("[Rashid] ", message, WEBHOOK_COLOR_ONLINE) --Sends webhook message
end

local rashid = GlobalEvent("rashid")
function rashid.onStartup()

	local today = os.date("*t").wday

	local config = positionByDay[today]
	if config then
		local rashid = Game.createNpc("Rashid", config.position)
		if rashid then
			rashid:setMasterPos(config.position)
			rashid:getPosition():sendMagicEffect(CONST_ME_TELEPORT)
		end

		logger.info("Rashid arrived at {}", config.city)
		local message = string.format("Rashid arrived at %s today.", config.city) -- Declaring the message to send to webhook.
		addEvent(rashidwebhook, 60000, message) -- Event with 1 minute delay to send webhook message after server starts.
	else
		logger.warn("[rashid.onStartup] - Cannot create Rashid. Day: {}",
			os.date("%A"))
	end

	return true

end
rashid:register()

local rashidSpawnOnTime = GlobalEvent("rashidSpawnOnTime")
function rashidSpawnOnTime.onTime(interval)

	local today = os.date("*t").wday

	local rashidTarget = Npc("rashid")
	local config = positionByDay[today]

	if rashidTarget then
		logger.info("Rashid is traveling to {}s location.", config.city)
		local message = ("Rashid is traveling to " .. config.city .. "s location.")
		rashidTarget:getPosition():sendMagicEffect(CONST_ME_TELEPORT)
		rashidTarget:teleportTo(config.position)
		rashidTarget:setMasterPos(config.position)
		rashidTarget:getPosition():sendMagicEffect(CONST_ME_TELEPORT)
		addEvent(rashidwebhook, 60000, message) -- Event with 1 minute delay to send webhook message after server starts.
	end

	return true

end
rashidSpawnOnTime:time("00:01")
rashidSpawnOnTime:register()
