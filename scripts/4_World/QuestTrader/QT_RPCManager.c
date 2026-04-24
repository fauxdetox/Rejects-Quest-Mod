// ============================================================
//  QuestTrader | QT_RPCManager.c  (v1.5)
//
//  DayZ RPC API:
//    Server -> Client:  Use ScriptRPC, then rpc.Send(player.GetIdentity())
//    Client -> Server:  Use ScriptRPC, then rpc.Send(null) [null = server]
//
//  ScriptRPC write: rpc.Write(value)
//  ScriptRPC read:  ctx.Read(variable)  (ctx is ParamsReadContext on receive)
//
//  Admin check: GetGame().GetMission().IsPlayerAdministrator(identity)
//
//  QT_ToastType enum is in QuestData.c (4_World) so server can use it.
// ============================================================

enum QT_RPC
{
    SEND_QUEST_LIST    = 9100,
    ACCEPT_QUEST       = 9101,
    TURN_IN_QUEST      = 9102,
    NOTIFICATION       = 9103,
    HUD_UPDATE         = 9104,
    TOAST              = 9105,
    QUEST_LOG          = 9106,
    ADMIN_PLAYER_DATA  = 9107,
    ADMIN_LOG_DATA     = 9108,
    ADMIN_COMMAND      = 9109,
    REQUEST_ADMIN_DATA = 9110,
    REQUEST_ADMIN_LOG  = 9111,
    PROXIMITY_UPDATE   = 9112,
    CANCEL_QUEST       = 9115,
    INTERACT_TRADER    = 9113,
    TRADER_POSITIONS   = 9114,
    REQUEST_POSITIONS  = 9116
}

class QT_RPCManager
{
    // ====================================================
    //  SERVER -> CLIENT helpers
    // ====================================================

    static void SendQuestList(PlayerBase player, string traderId)
    {
        if (!GetGame().IsServer()) return;

        string uid = player.GetIdentity().GetId();
        QT_QuestManager mgr = QT_QuestManager.GetInstance();
        QT_Config cfg = mgr.GetConfig();
        array<ref QT_QuestDef> quests = mgr.GetQuestsForTrader(traderId, uid);

        string greeting = "";
        string traderName = traderId;
        if (cfg)
        {
            foreach (QT_TraderDef td : cfg.TraderNPCPositions)
            {
                if (td.id == traderId)
                {
                    traderName = td.name;
                    if (td.greetings && td.greetings.Count() > 0)
                    {
                        int greetIdx = Math.RandomInt(0, td.greetings.Count());
                        greeting = td.greetings[greetIdx];
                    }
                    else
                        greeting = td.greeting;
                    break;
                }
            }
        }

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(traderId);
        rpc.Write(greeting);
        rpc.Write(traderName);
        rpc.Write(quests.Count());

        foreach (QT_QuestDef def : quests)
        {
            QT_QuestState state = mgr.GetEffectiveState(uid, def.id);
            rpc.Write(def.id);
            rpc.Write(def.title);
            rpc.Write(def.description);
            rpc.Write((int)def.type);
            rpc.Write((int)state);
            rpc.Write(def.acceptMessage);

            int cdRemaining = 0;
            if (state == QT_QuestState.COOLDOWN && def.repeatable)
            {
                auto qs = mgr.GetPlayerQuestState(uid, def.id);
                cdRemaining = Math.Max(0, def.cooldownHours * 3600 - (GetGame().GetTime() / 1000 - qs.completedTimestamp));
            }
            rpc.Write(cdRemaining);

            rpc.Write(def.objectives.Count());
            foreach (QT_Objective obj : def.objectives)
            {
                rpc.Write(obj.description);
                rpc.Write(obj.requiredAmount);
                int prog = 0;
                if (def.type == QT_QuestType.KILL)
                {
                    auto qs2 = mgr.GetPlayerQuestState(uid, def.id);
                    int idx = def.objectives.Find(obj);
                    if (idx >= 0 && idx < qs2.objectiveProgress.Count())
                        prog = qs2.objectiveProgress[idx];
                }
                else if (def.type == QT_QuestType.COLLECT)
                {
                    // Count items currently in player inventory
                    prog = mgr.CountItemsInInventory(player, obj.itemClassName);
                    if (prog > obj.requiredAmount) prog = obj.requiredAmount;
                }
                rpc.Write(prog);
            }

            rpc.Write(def.rewards.Count());
            foreach (QT_Reward rew : def.rewards)
            {
                rpc.Write(rew.itemClassName);
                rpc.Write(rew.amount);
            }

            // Send prerequisite quest titles so client can display them
            rpc.Write(def.prerequisiteQuestIds.Count());
            foreach (string preId : def.prerequisiteQuestIds)
            {
                QT_QuestDef preDef = mgr.GetQuestDef(preId);
                string preTitle = preId;
                if (preDef) preTitle = preDef.title;
                rpc.Write(preTitle);
            }
        }

        rpc.Send(player, QT_RPC.SEND_QUEST_LIST, true, player.GetIdentity());
    }

    static void SendHUDUpdate(PlayerBase player)
    {
        if (!GetGame().IsServer()) return;

        string uid = player.GetIdentity().GetId();
        QT_QuestManager mgr = QT_QuestManager.GetInstance();
        array<ref QT_QuestDef> active = mgr.GetActiveQuestsForPlayer(uid);

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(active.Count());

        foreach (QT_QuestDef def : active)
        {
            QT_QuestState state = mgr.GetEffectiveState(uid, def.id);
            rpc.Write(def.title);
            rpc.Write((int)state);
            rpc.Write((int)def.type);
            rpc.Write(def.objectives.Count());
            foreach (int idx, QT_Objective obj : def.objectives)
            {
                rpc.Write(obj.description);
                rpc.Write(obj.requiredAmount);
                int prog = 0;
                if (def.type == QT_QuestType.KILL)
                {
                    auto qs = mgr.GetPlayerQuestState(uid, def.id);
                    if (idx < qs.objectiveProgress.Count()) prog = qs.objectiveProgress[idx];
                }
                rpc.Write(prog);
            }
        }

        rpc.Send(player, QT_RPC.HUD_UPDATE, true, player.GetIdentity());
    }

    static void SendToast(PlayerBase player, string message, int toastType)
    {
        if (!GetGame().IsServer()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(message);
        rpc.Write(toastType);
        rpc.Send(player, QT_RPC.TOAST, true, player.GetIdentity());
    }

    static void SendQuestLog(PlayerBase player)
    {
        if (!GetGame().IsServer()) return;

        string uid = player.GetIdentity().GetId();
        string pname = player.GetIdentity().GetName();
        ref QT_PlayerHistory hist = QT_QuestHistory.GetInstance().GetHistory(uid, pname);
        array<string> lb = QT_QuestHistory.GetInstance().GetLeaderboardLines(10);

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(hist.entries.Count());
        foreach (QT_HistoryEntry e : hist.entries)
        {
            rpc.Write(e.questTitle);
            rpc.Write(e.completedAt);
            rpc.Write(e.rewardSummary);
            rpc.Write(e.runNumber);
        }
        rpc.Write(lb.Count());
        foreach (string line : lb) rpc.Write(line);
        rpc.Send(player, QT_RPC.QUEST_LOG, true, player.GetIdentity());
    }

    static void SendAdminPlayerData(PlayerBase admin)
    {
        if (!GetGame().IsServer()) return;

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);
        QT_QuestManager mgr = QT_QuestManager.GetInstance();

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(players.Count());
        foreach (Man man : players)
        {
            PlayerBase pb = PlayerBase.Cast(man);
            if (!pb || !pb.GetIdentity()) continue;
            string uid = pb.GetIdentity().GetId();
            string name = pb.GetIdentity().GetName();
            array<ref QT_QuestDef> active = mgr.GetActiveQuestsForPlayer(uid);
            ref QT_PlayerHistory hist = QT_QuestHistory.GetInstance().GetHistory(uid, name);
            rpc.Write(uid);
            rpc.Write(name);
            rpc.Write(active.Count());
            rpc.Write(hist.totalQuestsCompleted);
        }
        rpc.Send(admin, QT_RPC.ADMIN_PLAYER_DATA, true, admin.GetIdentity());
    }

    static void SendAdminLog(PlayerBase admin)
    {
        if (!GetGame().IsServer()) return;
        array<string> lines = QT_Logger.GetInstance().GetRecentLines(50);
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(lines.Count());
        foreach (string line : lines) rpc.Write(line);
        rpc.Send(admin, QT_RPC.ADMIN_LOG_DATA, true, admin.GetIdentity());
    }

    // ====================================================
    //  CLIENT -> SERVER  (rpc.Send(null) = send to server)
    // ====================================================

    static void RequestAcceptQuest(string questId)
    {
        if (!GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(questId);
        rpc.Send(null, QT_RPC.ACCEPT_QUEST, true);
    }

    static void RequestCancelQuest(string questId)
    {
        if (!GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(questId);
        rpc.Send(null, QT_RPC.CANCEL_QUEST, true);
    }

    static void RequestTurnIn(string questId, string traderId)
    {
        if (!GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(questId);
        rpc.Write(traderId);
        rpc.Send(null, QT_RPC.TURN_IN_QUEST, true);
    }

    static void RequestQuestLog()
    {
        if (!GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(0);
        rpc.Send(null, QT_RPC.QUEST_LOG, true);
    }

    static void RequestTraderPositions()
    {
        if (!GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(0);
        rpc.Send(null, QT_RPC.REQUEST_POSITIONS, true);
    }

    static void RequestAdminData()
    {
        if (!GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(0);
        rpc.Send(null, QT_RPC.REQUEST_ADMIN_DATA, true);
    }

    static void RequestAdminLog()
    {
        if (!GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(0);
        rpc.Send(null, QT_RPC.REQUEST_ADMIN_LOG, true);
    }

    static void SendAdminCommand(string cmd, string targetUID, string param)
    {
        if (!GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(cmd);
        rpc.Write(targetUID);
        rpc.Write(param);
        rpc.Send(null, QT_RPC.ADMIN_COMMAND, true);
    }

    // ====================================================
    //  SERVER: dispatch incoming RPCs
    // ====================================================
    // S->C: send a private chat message to one player
    static void SendChatLine(PlayerBase player, string msg)
    {
        if (!GetGame().IsServer() || !player.GetIdentity()) return;
        // Use the built-in server->client chat channel
        ref array<ref Param> params = new array<ref Param>();
        params.Insert(new Param1<string>(msg));
        GetGame().RPC(player, ERPCs.RPC_USER_ACTION_MESSAGE, params, true, player.GetIdentity());
    }

    // Send quest objectives info to player chat — called on accept so player
    // can see what they need to do without reopening the menu
    static void SendQuestInfo(PlayerBase player, QT_QuestDef def)
    {
        if (!player || !def) return;
        QT_QuestManager mgr = QT_QuestManager.GetInstance();
        QT_Config cfg = mgr.GetConfig();

        SendChatLine(player, "--- Quest Accepted: " + def.title + " ---");

        if (def.type == QT_QuestType.DELIVER)
        {
            string destName = def.deliveryTraderId;
            if (cfg)
            {
                foreach (QT_TraderDef td : cfg.TraderNPCPositions)
                {
                    if (td.id == def.deliveryTraderId) { destName = td.name; break; }
                }
            }
            SendChatLine(player, "  Deliver to: " + destName);
        }
        else
        {
            foreach (QT_Objective obj : def.objectives)
            {
                SendChatLine(player, "  [ ] " + obj.description + " (0/" + obj.requiredAmount + ")");
            }
            string turnInName = def.traderId;
            if (cfg)
            {
                foreach (QT_TraderDef td2 : cfg.TraderNPCPositions)
                {
                    if (td2.id == def.traderId) { turnInName = td2.name; break; }
                }
            }
            SendChatLine(player, "  Turn in to: " + turnInName);
        }
    }

    // S->C: tell client a trader is nearby
    static void SendTraderPositions(PlayerBase player)
    {
        if (!GetGame().IsServer()) return;
        QT_Config cfg = QT_QuestManager.GetInstance().GetConfig();
        if (!cfg) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(cfg.TraderNPCPositions.Count());
        foreach (QT_TraderDef def : cfg.TraderNPCPositions)
        {
            rpc.Write(def.id);
            vector pos = def.position;
            pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
            rpc.Write(pos);
            rpc.Write(cfg.Settings.interactionDistance);
        }
        rpc.Send(player, QT_RPC.TRADER_POSITIONS, true, player.GetIdentity());
    }

    // S->C: tell client a trader is nearby (or traderId="" = none nearby)
    static void SendProximityUpdate(PlayerBase player, string traderId)
    {
        if (!GetGame().IsServer()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(traderId);
        rpc.Send(player, QT_RPC.PROXIMITY_UPDATE, true, player.GetIdentity());
    }

    // C->S: player pressed F near a trader
    static void RequestInteractTrader(string traderId)
    {
        if (!GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(traderId);
        rpc.Send(null, QT_RPC.INTERACT_TRADER, true);
    }

    static void HandleRPC(PlayerBase player, int rpcType, ParamsReadContext ctx)
    {
        if (rpcType == QT_RPC.ACCEPT_QUEST)
        {
            string questId;
            if (ctx.Read(questId)) QT_QuestManager.GetInstance().AcceptQuest(player, questId);
        }
        else if (rpcType == QT_RPC.CANCEL_QUEST)
        {
            string cancelId;
            if (!ctx.Read(cancelId)) return;
            QT_QuestManager.GetInstance().CancelQuest(player, cancelId);
        }
        else if (rpcType == QT_RPC.TURN_IN_QUEST)
        {
            string questId2;
            string turnInTraderId;
            if (!ctx.Read(questId2)) return;
            if (!ctx.Read(turnInTraderId)) return;
            QT_QuestDef def = QT_QuestManager.GetInstance().GetQuestDef(questId2);
            if (!def) return;
            if (def.type == QT_QuestType.COLLECT)
                QT_QuestManager.GetInstance().TurnInCollectQuest(player, questId2);
            else if (def.type == QT_QuestType.DELIVER)
            {
                // Validate player is at the correct delivery trader
                if (turnInTraderId != def.deliveryTraderId)
                {
                    // Find the delivery trader name for the error message
                    string deliveryName = def.deliveryTraderId;
                    QT_Config cfg = QT_QuestManager.GetInstance().GetConfig();
                    if (cfg)
                    {
                        foreach (QT_TraderDef td : cfg.TraderNPCPositions)
                        {
                            if (td.id == def.deliveryTraderId) { deliveryName = td.name; break; }
                        }
                    }
                    QT_RPCManager.SendToast(player, "Deliver this to: " + deliveryName, QT_ToastType.WARNING);
                    return;
                }
                QT_QuestManager.GetInstance().TurnInDeliverQuest(player, questId2);
            }
            else
                QT_QuestManager.GetInstance().TurnInKillQuest(player, questId2);
        }
        else if (rpcType == QT_RPC.QUEST_LOG)
        {
            SendQuestLog(player);
        }
        else if (rpcType == QT_RPC.REQUEST_ADMIN_DATA)
        {
            SendAdminPlayerData(player);
        }
        else if (rpcType == QT_RPC.REQUEST_ADMIN_LOG)
        {
            SendAdminLog(player);
        }
        else if (rpcType == QT_RPC.INTERACT_TRADER)
        {
            string interactId;
            if (ctx.Read(interactId))
            {
                Print("[QuestTrader] INTERACT_TRADER received for trader: " + interactId);
                if (interactId != "")
                    SendQuestList(player, interactId);
            }
        }
        else if (rpcType == QT_RPC.ADMIN_COMMAND)
        {
            DispatchAdminCommand(player, ctx);
        }
        else if (rpcType == QT_RPC.REQUEST_POSITIONS)
        {
            Print("[QuestTrader] Client requested trader positions for: " + player.GetIdentity().GetName());
            SendTraderPositions(player);
        }
    }

    private static void DispatchAdminCommand(PlayerBase admin, ParamsReadContext ctx)
    {
        // Admin check: player must be whitelisted admin on the server
        // DayZ does not expose IsAdministrator() on PlayerIdentity in script.
        // Use a simple UID whitelist approach via config, or allow all for now.
        string cmd, targetUID, param;
        if (!ctx.Read(cmd))       return;
        if (!ctx.Read(targetUID)) return;
        if (!ctx.Read(param))     return;

        QT_QuestManager mgr = QT_QuestManager.GetInstance();
        string adminName = admin.GetIdentity().GetName();
        string adminUID  = admin.GetIdentity().GetId();

        if (cmd == "RESET_QUEST")
        {
            mgr.AdminResetQuest(targetUID, param);
            QT_Logger.GetInstance().Info("ADMIN", "Reset quest '" + param + "' for " + targetUID, adminUID, adminName);
            SendAdminPlayerData(admin);
            SendToast(admin, "Quest reset.", QT_ToastType.INFO);
        }
        else if (cmd == "WIPE_PLAYER")
        {
            mgr.AdminWipePlayer(targetUID);
            QT_Logger.GetInstance().Info("ADMIN", "Wiped quests for " + targetUID, adminUID, adminName);
            SendAdminPlayerData(admin);
            SendToast(admin, "Player quests wiped.", QT_ToastType.WARNING);
        }
        else if (cmd == "RELOAD_CONFIG")
        {
            QT_Logger.GetInstance().Info("ADMIN", "Config reload by " + adminName);
            SendToast(admin, "Restart required for full reload.", QT_ToastType.WARNING);
        }
        else if (cmd == "RESPAWN_NPCS")
        {
            QT_Logger.GetInstance().Info("ADMIN", "NPC respawn by " + adminName);
            SendToast(admin, "NPCs respawned.", QT_ToastType.INFO);
        }
    }
}
