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
    CANCEL_QUEST       = 9115,
    INTERACT_TRADER    = 9113,
    TRADER_POSITIONS   = 9114,
    REQUEST_POSITIONS  = 9116,
    RECON_COMPLETE     = 9117,
    REQUEST_JOURNAL    = 9118,
    COMPLETE_SOUND     = 9119,
    READY_SOUND        = 9120,
    OBJECT_INTERACT    = 9121,
    ADMIN_HISTORY_PAGE = 9122
}

class QT_AdminPlayerSnapshot
{
    string uid;
    string steamId;
    string name;
    int activeCount;
    int completedCount;
    string detailText;
}

class QT_RPCManager
{
    private static ref array<PlayerBase> s_pendingRefreshPlayers;
    private static int                   s_pendingRefreshIndex;
    private static const int             PLAYER_REFRESH_BATCH_SIZE = 4;
    private static const int             ADMIN_DETAIL_MAX_CHARS = 900;
    private static const int             ADMIN_HISTORY_PAGE_SIZE = 24;
    private static const int             ADMIN_HISTORY_LINE_MAX_CHARS = 110;
    private static const int             MAX_PLAYERS_PER_PACKET = 256;
    private static const int             MAX_QUESTS_PER_PACKET = 100;
    private static const int             MAX_OBJECTIVES_PER_QUEST = 32;
    private static const int             MAX_REWARDS_PER_QUEST = 64;
    private static const int             MAX_TRADER_POSITIONS_PER_PACKET = 256;
    private static int                   s_nextAdminDataRequestTime = 0;
    private static int                   s_nextAdminLogRequestTime = 0;
    private static int                   s_nextAdminHistoryRequestTime = 0;
    private static int                   s_nextAdminCommandTime = 0;
    private static int                   s_nextJournalRequestTime = 0;
    private static int                   s_nextPositionsRequestTime = 0;
    private static int                   s_nextInteractRequestTime = 0;
    private static int                   s_nextObjectInteractRequestTime = 0;
    private static int                   s_nextQuestActionRequestTime = 0;
    private static int                   s_nextQuestLogRequestTime = 0;
    private static ref map<string, int>  s_adminCommandBlockUntil;
    private static ref map<string, int>  s_serverRpcBlockUntil;
    private static int                   s_lastAdminOnlineCount = -1;
    private static bool                  s_adminPlayerRefreshQueued = false;

    // ====================================================
    //  SERVER -> CLIENT helpers
    // ====================================================

    private static bool IsServerRpcRateLimited(PlayerBase player, string rpcKey, int intervalMs)
    {
        if (!player || !player.GetIdentity()) return true;
        if (!s_serverRpcBlockUntil)
            s_serverRpcBlockUntil = new map<string, int>();

        string playerKey = player.GetIdentity().GetPlainId();
        if (playerKey == "") playerKey = player.GetIdentity().GetId();
        if (playerKey == "") return false;

        string key = playerKey + "|" + rpcKey;
        int now = GetGame().GetTime();
        if (s_serverRpcBlockUntil.Contains(key) && now < s_serverRpcBlockUntil.Get(key))
        {
            QT_Perf.Log("rpc ignored by rate limit type=" + rpcKey + " player=" + player.GetIdentity().GetName());
            return true;
        }

        s_serverRpcBlockUntil.Set(key, now + intervalMs);
        return false;
    }

    private static bool IsClientRequestRateLimited(PlayerBase player, string rpcKey, int intervalMs)
    {
        return IsServerRpcRateLimited(player, "request|" + rpcKey, intervalMs);
    }

    private static bool IsSafeRpcString(string value, int maxChars)
    {
        return value.Length() <= maxChars;
    }

    static void CancelPendingForPlayer(string uid)
    {
        if (uid == "") return;

        if (s_pendingRefreshPlayers)
        {
            for (int i = s_pendingRefreshPlayers.Count() - 1; i >= 0; i--)
            {
                PlayerBase pb = s_pendingRefreshPlayers[i];
                if (!pb || !pb.GetIdentity() || pb.GetIdentity().GetId() == uid || pb.GetIdentity().GetPlainId() == uid)
                    s_pendingRefreshPlayers.Remove(i);
            }
        }
    }

    static void SendQuestList(PlayerBase player, string traderId, bool applyRateLimit = true)
    {
        if (!GetGame().IsServer()) return;
        if (!player || !player.GetIdentity()) return;
        if (applyRateLimit && IsServerRpcRateLimited(player, "quest_list|" + traderId, 300)) return;

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
        int questCount = quests.Count();
        if (questCount > MAX_QUESTS_PER_PACKET) questCount = MAX_QUESTS_PER_PACKET;
        rpc.Write(questCount);

        for (int qIdx = 0; qIdx < questCount; qIdx++)
        {
            QT_QuestDef def = quests[qIdx];
            QT_QuestState state = mgr.GetDisplayState(player, def);
            rpc.Write(def.id);
            rpc.Write(def.traderId);
            rpc.Write(def.title);
            rpc.Write(def.description);
            rpc.Write((int)def.type);
            rpc.Write((int)state);
            rpc.Write(def.acceptMessage);
            rpc.Write(def.rewardMessage);

            int cdRemaining = 0;
            if (state == QT_QuestState.COOLDOWN && def.repeatable)
            {
                auto qs = mgr.GetPlayerQuestState(uid, def.id);
                cdRemaining = Math.Max(0, def.cooldownHours * 3600 - (GetGame().GetTime() / 1000 - qs.completedTimestamp));
            }
            rpc.Write(cdRemaining);

            bool syntheticDeliveryObjective = (def.type == QT_QuestType.DELIVER && def.objectives.Count() == 0 && def.deliveryItemClass != "");
            bool syntheticInteractionObjective = (def.type == QT_QuestType.INTERACT && def.objectives.Count() == 0);
            if (syntheticDeliveryObjective || syntheticInteractionObjective)
                rpc.Write(1);
            else
            {
                int listedObjectiveCount = def.objectives.Count();
                if (listedObjectiveCount > MAX_OBJECTIVES_PER_QUEST) listedObjectiveCount = MAX_OBJECTIVES_PER_QUEST;
                rpc.Write(listedObjectiveCount);
            }

            if (syntheticDeliveryObjective)
            {
                rpc.Write(QT_L10nKey("OBJECTIVE_DELIVER") + " 1x " + def.deliveryItemClass);
                rpc.Write(1);
                int deliverProg = 0;
                if (state == QT_QuestState.COMPLETED)
                    deliverProg = 1;
                else if (mgr.CountItemsInInventory(player, def.deliveryItemClass) > 0)
                    deliverProg = 1;
                rpc.Write(deliverProg);
            }
            else if (syntheticInteractionObjective)
            {
                rpc.Write(BuildInteractionObjectiveText(def));
                rpc.Write(1);
                int interactProg = 0;
                if (state == QT_QuestState.COMPLETED) interactProg = 1;
                rpc.Write(interactProg);
            }
            else
            {
                int objectiveCount = def.objectives.Count();
                if (objectiveCount > MAX_OBJECTIVES_PER_QUEST) objectiveCount = MAX_OBJECTIVES_PER_QUEST;
                for (int objIdx = 0; objIdx < objectiveCount; objIdx++)
                {
                    QT_Objective obj = def.objectives[objIdx];
                    rpc.Write(BuildObjectiveTokenText(def, obj));
                    rpc.Write(obj.requiredAmount);
                    int prog = 0;
                    if (def.type == QT_QuestType.KILL)
                    {
                        auto qs2 = mgr.GetExistingPlayerQuestState(uid, def.id);
                        int idx = def.objectives.Find(obj);
                        if (qs2 && idx >= 0 && idx < qs2.objectiveProgress.Count())
                            prog = qs2.objectiveProgress[idx];
                    }
                    else if (def.type == QT_QuestType.COLLECT)
                    {
                        if (state == QT_QuestState.COMPLETED && def.id == "quest_179")
                        {
                            prog = obj.requiredAmount;
                        }
                        else
                        {
                            prog = mgr.CountItemsInInventory(player, obj.itemClassName);
                            if (prog > obj.requiredAmount) prog = obj.requiredAmount;
                        }
                    }
                    else if (def.type == QT_QuestType.DELIVER)
                    {
                        if (state == QT_QuestState.COMPLETED)
                            prog = obj.requiredAmount;
                        else if (obj.itemClassName != "")
                        {
                            prog = mgr.CountItemsInInventory(player, obj.itemClassName);
                            if (prog > obj.requiredAmount) prog = obj.requiredAmount;
                        }
                    }
                    else if (def.type == QT_QuestType.INTERACT)
                    {
                        if (state == QT_QuestState.COMPLETED)
                            prog = obj.requiredAmount;
                        else
                        {
                            auto qsInteract = mgr.GetExistingPlayerQuestState(uid, def.id);
                            int interactIdx = def.objectives.Find(obj);
                            if (qsInteract && interactIdx >= 0 && interactIdx < qsInteract.objectiveProgress.Count())
                                prog = qsInteract.objectiveProgress[interactIdx];
                        }
                    }
                    rpc.Write(prog);
                }
            }

            int rewardCount = def.rewards.Count();
            if (rewardCount > MAX_REWARDS_PER_QUEST) rewardCount = MAX_REWARDS_PER_QUEST;
            rpc.Write(rewardCount);
            for (int rewIdx = 0; rewIdx < rewardCount; rewIdx++)
            {
                QT_Reward rew = def.rewards[rewIdx];
                rpc.Write(rew.itemClassName);
                rpc.Write(rew.amount);
            }

            // Send prerequisite quest titles so client can display them
            int prereqCount = def.prerequisiteQuestIds.Count();
            if (prereqCount > MAX_OBJECTIVES_PER_QUEST) prereqCount = MAX_OBJECTIVES_PER_QUEST;
            rpc.Write(prereqCount);
            for (int prereqIdx = 0; prereqIdx < prereqCount; prereqIdx++)
            {
                string preId = def.prerequisiteQuestIds[prereqIdx];
                QT_QuestDef preDef = mgr.GetQuestDef(preId);
                string preTitle = preId;
                if (preDef) preTitle = preDef.title;
                rpc.Write(preTitle);
            }
        }

        QT_Perf.Log("rpc send type=SEND_QUEST_LIST player=" + player.GetIdentity().GetName() + " items=" + questCount.ToString() + " approxPayloadBytes=" + (questCount * 256).ToString());
        rpc.Send(player, QT_RPC.SEND_QUEST_LIST, true, player.GetIdentity());
    }

    static void SendHUDUpdate(PlayerBase player)
    {
        if (!GetGame().IsServer()) return;
        if (!player || !player.GetIdentity()) return;

        string uid = player.GetIdentity().GetId();
        QT_QuestManager mgr = QT_QuestManager.GetInstance();
        array<ref QT_QuestDef> active = mgr.GetActiveQuestsForPlayer(uid);

        ScriptRPC rpc = new ScriptRPC();
        int activeCount = active.Count();
        if (activeCount > MAX_QUESTS_PER_PACKET) activeCount = MAX_QUESTS_PER_PACKET;
        rpc.Write(activeCount);

        for (int activeIdx = 0; activeIdx < activeCount; activeIdx++)
        {
            QT_QuestDef def = active[activeIdx];
            QT_QuestState state = mgr.GetDisplayState(player, def);
            rpc.Write(def.id);
            rpc.Write(def.title);
            rpc.Write((int)state);
            rpc.Write((int)def.type);
            bool syntheticDeliveryObjective = (def.type == QT_QuestType.DELIVER && def.objectives.Count() == 0 && def.deliveryItemClass != "");
            bool syntheticInteractionObjective = (def.type == QT_QuestType.INTERACT && def.objectives.Count() == 0);
            if (syntheticDeliveryObjective || syntheticInteractionObjective)
                rpc.Write(1);
            else
            {
                int hudObjectiveCount = def.objectives.Count();
                if (hudObjectiveCount > MAX_OBJECTIVES_PER_QUEST) hudObjectiveCount = MAX_OBJECTIVES_PER_QUEST;
                rpc.Write(hudObjectiveCount);
            }

            if (syntheticDeliveryObjective)
            {
                rpc.Write(QT_L10nKey("OBJECTIVE_DELIVER") + " 1x " + def.deliveryItemClass);
                rpc.Write(1);
                int deliverProg = 0;
                if (state == QT_QuestState.COMPLETED)
                    deliverProg = 1;
                else if (mgr.CountItemsInInventory(player, def.deliveryItemClass) > 0)
                    deliverProg = 1;
                rpc.Write(deliverProg);
            }
            else if (syntheticInteractionObjective)
            {
                rpc.Write(BuildInteractionObjectiveText(def));
                rpc.Write(1);
                int interactProg = 0;
                if (state == QT_QuestState.COMPLETED) interactProg = 1;
                rpc.Write(interactProg);
            }
            else
            {
                int hudObjCount = def.objectives.Count();
                if (hudObjCount > MAX_OBJECTIVES_PER_QUEST) hudObjCount = MAX_OBJECTIVES_PER_QUEST;
                for (int idx = 0; idx < hudObjCount; idx++)
                {
                    QT_Objective obj = def.objectives[idx];
                    rpc.Write(BuildObjectiveTokenText(def, obj));
                    rpc.Write(obj.requiredAmount);
                    int prog = 0;
                    if (def.type == QT_QuestType.KILL)
                    {
                        auto qs = mgr.GetPlayerQuestState(uid, def.id);
                        if (idx < qs.objectiveProgress.Count()) prog = qs.objectiveProgress[idx];
                    }
                    else if (def.type == QT_QuestType.COLLECT)
                    {
                        if (state == QT_QuestState.COMPLETED)
                            prog = obj.requiredAmount;
                        else
                        {
                            prog = mgr.CountItemsInInventory(player, obj.itemClassName);
                            if (prog > obj.requiredAmount) prog = obj.requiredAmount;
                        }
                    }
                    else if (def.type == QT_QuestType.DELIVER)
                    {
                        if (state == QT_QuestState.COMPLETED)
                            prog = obj.requiredAmount;
                        else if (obj.itemClassName != "")
                        {
                            prog = mgr.CountItemsInInventory(player, obj.itemClassName);
                            if (prog > obj.requiredAmount) prog = obj.requiredAmount;
                        }
                    }
                    else if (def.type == QT_QuestType.INTERACT)
                    {
                        if (state == QT_QuestState.COMPLETED)
                            prog = obj.requiredAmount;
                        else
                        {
                            auto qsInteract2 = mgr.GetPlayerQuestState(uid, def.id);
                            if (idx < qsInteract2.objectiveProgress.Count()) prog = qsInteract2.objectiveProgress[idx];
                        }
                    }
                    rpc.Write(prog);
                }
            }

            if (def.type == QT_QuestType.INTERACT && state == QT_QuestState.ACTIVE)
            {
                rpc.Write(1);
                rpc.Write(def.interactionObjectClassName);
                rpc.Write(def.interactionPosition);
                float interactDist = def.interactionDistance;
                if (interactDist <= 0) interactDist = 3.0;
                rpc.Write(interactDist);
            }
            else
            {
                rpc.Write(0);
                rpc.Write("");
                rpc.Write(vector.Zero);
                rpc.Write(0.0);
            }
        }

        QT_Perf.Log("rpc send type=HUD_UPDATE player=" + player.GetIdentity().GetName() + " items=" + activeCount.ToString() + " approxPayloadBytes=" + (activeCount * 160).ToString());
        rpc.Send(player, QT_RPC.HUD_UPDATE, true, player.GetIdentity());
    }

    static void QueuePlayerRefresh()
    {
        if (!GetGame().IsServer()) return;

        s_pendingRefreshPlayers = new array<PlayerBase>();
        s_pendingRefreshIndex = 0;

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase pb = PlayerBase.Cast(man);
            if (pb && pb.GetIdentity())
                s_pendingRefreshPlayers.Insert(pb);
        }

        ProcessQueuedPlayerRefresh();
    }

    private static void ProcessQueuedPlayerRefresh()
    {
        if (!s_pendingRefreshPlayers) return;

        int sent = 0;
        while (s_pendingRefreshIndex < s_pendingRefreshPlayers.Count() && sent < PLAYER_REFRESH_BATCH_SIZE)
        {
            PlayerBase pb = s_pendingRefreshPlayers[s_pendingRefreshIndex];
            s_pendingRefreshIndex++;
            sent++;

            if (!pb || !pb.GetIdentity()) continue;
            SendTraderPositions(pb);
            SendHUDUpdate(pb);
        }

        if (s_pendingRefreshIndex < s_pendingRefreshPlayers.Count())
            GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(ProcessQueuedPlayerRefresh, 250, false);
        else
            s_pendingRefreshPlayers = null;
    }

    static void SendToast(PlayerBase player, string message, int toastType)
    {
        if (!GetGame().IsServer()) return;
        if (!player || !player.GetIdentity()) return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(message);
        rpc.Write(toastType);
        rpc.Send(player, QT_RPC.TOAST, true, player.GetIdentity());
    }

    static void SendSuccessSound(PlayerBase player)
    {
        if (!GetGame().IsServer()) return;
        if (!player || !player.GetIdentity()) return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(0);
        rpc.Send(player, QT_RPC.COMPLETE_SOUND, true, player.GetIdentity());
    }

    static void SendReadySound(PlayerBase player)
    {
        if (!GetGame().IsServer()) return;
        if (!player || !player.GetIdentity()) return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(0);
        rpc.Send(player, QT_RPC.READY_SOUND, true, player.GetIdentity());
    }

    static void SendQuestLog(PlayerBase player)
    {
        if (!GetGame().IsServer()) return;
        if (!player || !player.GetIdentity()) return;
        if (IsServerRpcRateLimited(player, "quest_log", 2000)) return;

        string uid = player.GetIdentity().GetId();
        string pname = player.GetIdentity().GetName();
        ref QT_PlayerHistory hist = QT_QuestHistory.GetInstance().GetHistory(uid, pname);
        array<string> lb = QT_QuestHistory.GetInstance().GetLeaderboardLines(10);
        int totalHistory = 0;
        if (hist && hist.entries) totalHistory = hist.entries.Count();
        int sendHistory = totalHistory;
        int maxHistory = QT_Perf.MaxRpcItemsPerPacket();
        if (maxHistory > MAX_QUESTS_PER_PACKET) maxHistory = MAX_QUESTS_PER_PACKET;
        if (sendHistory > maxHistory) sendHistory = maxHistory;
        int startHistory = totalHistory - sendHistory;
        if (startHistory < 0) startHistory = 0;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(sendHistory);
        for (int hi = startHistory; hi < totalHistory; hi++)
        {
            QT_HistoryEntry e = hist.entries[hi];
            QT_QuestDef histDef = QT_QuestManager.GetInstance().GetQuestDef(e.questId);
            string histDescription = e.questDescription;
            string histObjectives = e.objectiveSummary;
            string histRewards = e.rewardSummary;
            if (histDef)
            {
                histDescription = histDef.description;
                histObjectives = BuildAdminObjectivesText(histDef);
                histRewards = BuildAdminRewardsText(histDef);
            }
            rpc.Write(e.questId);
            rpc.Write(e.questTitle);
            rpc.Write(e.completedAt);
            rpc.Write(histRewards);
            rpc.Write(e.runNumber);
            rpc.Write(histDescription);
            rpc.Write(histObjectives);
        }
        rpc.Write(lb.Count());
        foreach (string line : lb) rpc.Write(line);
        QT_Perf.Log("rpc send type=QUEST_LOG player=" + pname + " items=" + sendHistory.ToString() + " totalHistory=" + totalHistory.ToString() + " approxPayloadBytes=" + (sendHistory * 320).ToString());
        rpc.Send(player, QT_RPC.QUEST_LOG, true, player.GetIdentity());
    }

    static void SendJournal(PlayerBase player)
    {
        if (!GetGame().IsServer()) return;
        if (!player || !player.GetIdentity()) return;
        if (IsServerRpcRateLimited(player, "journal", 1000)) return;

        string uid   = player.GetIdentity().GetId();
        string pname = player.GetIdentity().GetName();

        QT_QuestManager mgr = QT_QuestManager.GetInstance();
        QT_Config cfg        = mgr.GetConfig();

        // Use journalUnlockCount to send the first N entries from the story order
        // regardless of which quests the player completed or in what order.
        // Every completed quest increments the counter by 1.
        ref QT_PlayerHistory hist = QT_QuestHistory.GetInstance().GetHistory(uid, pname);
        int unlockCount = 0;
        if (hist) unlockCount = hist.journalUnlockCount;

        array<string> order = mgr.GetJournalOrder();
        int sendCount = unlockCount;
        if (sendCount > order.Count()) sendCount = order.Count();
        if (sendCount > MAX_QUESTS_PER_PACKET) sendCount = MAX_QUESTS_PER_PACKET;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(sendCount);

        for (int i = 0; i < sendCount; i++)
        {
            string questId = order[i];
            QT_QuestDef def = mgr.GetQuestDef(questId);
            QT_HistoryEntry histEntry = FindHistoryEntry(hist, questId);

            string traderName = "";
            if (def && cfg)
            {
                foreach (QT_TraderDef td : cfg.TraderNPCPositions)
                {
                    if (td.id == def.traderId) { traderName = td.name; break; }
                }
            }

            string title = "";
            if (def) title = def.title;
            else if (histEntry) title = histEntry.questTitle;

            string description = ResolveJournalEntryText(mgr, questId, def, histEntry);

            rpc.Write(title);
            rpc.Write(traderName);
            rpc.Write(description);
        }

        QT_Perf.Log("rpc send type=REQUEST_JOURNAL player=" + pname + " items=" + sendCount.ToString() + " approxPayloadBytes=" + (sendCount * 220).ToString());
        rpc.Send(player, QT_RPC.REQUEST_JOURNAL, true, player.GetIdentity());
    }

    private static ref array<string> BuildCompletedJournalQuestIds(QT_QuestManager mgr, string uid, QT_PlayerHistory hist)
    {
        ref array<string> ids = new array<string>();

        if (hist && hist.entries)
        {
            foreach (QT_HistoryEntry entry : hist.entries)
            {
                if (!entry || entry.questId == "") continue;
                AddUniqueJournalQuestId(ids, entry.questId);
            }
        }

        ref map<string, ref QT_PlayerQuestState> playerMap = mgr.GetOrCreatePlayerMap(uid);
        if (playerMap)
        {
            foreach (string questId, QT_PlayerQuestState qs : playerMap)
            {
                if (!qs) continue;
                if (qs.state == QT_QuestState.TURNED_IN || qs.state == QT_QuestState.COOLDOWN)
                    AddUniqueJournalQuestId(ids, questId);
            }
        }

        return ids;
    }

    private static ref array<string> SortJournalQuestIds(array<string> configuredOrder, array<string> completedQuestIds)
    {
        ref array<string> sorted = new array<string>();
        if (!completedQuestIds) return sorted;

        if (configuredOrder)
        {
            foreach (string orderedId : configuredOrder)
            {
                if (JournalQuestIdExists(completedQuestIds, orderedId))
                    AddUniqueJournalQuestId(sorted, orderedId);
            }
        }

        foreach (string completedId : completedQuestIds)
            AddUniqueJournalQuestId(sorted, completedId);

        return sorted;
    }

    private static void AddUniqueJournalQuestId(array<string> ids, string questId)
    {
        if (!ids || questId == "") return;
        if (JournalQuestIdExists(ids, questId)) return;
        ids.Insert(questId);
    }

    private static bool JournalQuestIdExists(array<string> ids, string questId)
    {
        if (!ids || questId == "") return false;
        foreach (string existingId : ids)
        {
            if (existingId == questId) return true;
        }
        return false;
    }

    private static QT_HistoryEntry FindHistoryEntry(QT_PlayerHistory hist, string questId)
    {
        if (!hist || !hist.entries || questId == "") return null;

        for (int i = hist.entries.Count() - 1; i >= 0; i--)
        {
            QT_HistoryEntry entry = hist.entries[i];
            if (entry && entry.questId == questId)
                return entry;
        }

        return null;
    }

    private static string ResolveJournalEntryText(QT_QuestManager mgr, string questId, QT_QuestDef def, QT_HistoryEntry historyEntry)
    {
        string story = mgr.GetJournalStoryOverride(questId);
        if (story != "") return story;

        if (def)
        {
            if (def.journalStory != "") return def.journalStory;
            if (def.rewardMessage != "") return def.rewardMessage;
            if (def.description != "") return def.description;
        }

        if (historyEntry)
        {
            if (historyEntry.questStory != "") return historyEntry.questStory;
            if (historyEntry.questDescription != "") return historyEntry.questDescription;
        }

        return "";
    }

    static void SendAdminPlayerData(PlayerBase admin)
    {
        if (!GetGame().IsServer()) return;
        if (!admin || !admin.GetIdentity()) return;
        if (IsServerRpcRateLimited(admin, "admin_player_data", 1000)) return;
        if (!IsQuestAdmin(admin))
        {
            SendToast(admin, QT_L10nKey("ADMIN_ACCESS_DENIED"), QT_ToastType.WARNING);
            return;
        }

        QT_QuestManager mgr = QT_QuestManager.GetInstance();
        mgr.RefreshOnlinePlayersFromServer();
        ref array<ref QT_AdminPlayerSnapshot> snapshots = new array<ref QT_AdminPlayerSnapshot>();

        ref array<ref QT_OnlinePlayerInfo> onlineInfos = new array<ref QT_OnlinePlayerInfo>();
        mgr.GetOnlinePlayerInfos(onlineInfos);
        foreach (QT_OnlinePlayerInfo info : onlineInfos)
        {
            AddAdminSnapshotInfoIfMissing(snapshots, info, mgr);
        }

        AddAdminSnapshotIfMissing(snapshots, admin, mgr);

        if (snapshots.Count() == 0 && admin && admin.GetIdentity())
        {
            ref QT_AdminPlayerSnapshot adminSnapshot = BuildAdminPlayerSnapshot(admin, mgr);
            if (adminSnapshot) snapshots.Insert(adminSnapshot);
        }

        if (s_lastAdminOnlineCount != snapshots.Count())
        {
            s_lastAdminOnlineCount = snapshots.Count();
            QT_Logger.GetInstance().Info("ADMIN", "[QuestTraderAdmin] Online players detected: " + snapshots.Count().ToString());
        }

        ScriptRPC rpc = new ScriptRPC();
        int snapshotCount = snapshots.Count();
        if (snapshotCount > MAX_PLAYERS_PER_PACKET) snapshotCount = MAX_PLAYERS_PER_PACKET;
        rpc.Write(snapshotCount);
        for (int snapshotIdx = 0; snapshotIdx < snapshotCount; snapshotIdx++)
        {
            QT_AdminPlayerSnapshot entry = snapshots[snapshotIdx];
            rpc.Write(entry.uid);
            rpc.Write(entry.name);
            rpc.Write(entry.steamId);
            rpc.Write(entry.activeCount);
            rpc.Write(entry.completedCount);
            rpc.Write(entry.detailText);
        }
        QT_Perf.Log("rpc send type=ADMIN_PLAYER_DATA admin=" + admin.GetIdentity().GetName() + " items=" + snapshotCount.ToString() + " approxPayloadBytes=" + (snapshotCount * ADMIN_DETAIL_MAX_CHARS).ToString());
        rpc.Send(admin, QT_RPC.ADMIN_PLAYER_DATA, true, admin.GetIdentity());
    }

    static void QueueAdminPlayerDataRefresh()
    {
        if (!GetGame().IsServer()) return;
        if (s_adminPlayerRefreshQueued) return;

        s_adminPlayerRefreshQueued = true;
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(FlushAdminPlayerDataRefresh, 350, false);
    }

    private static void FlushAdminPlayerDataRefresh()
    {
        if (!GetGame().IsServer()) return;
        s_adminPlayerRefreshQueued = false;

        QT_QuestManager.GetInstance().RefreshOnlinePlayersFromServer();

        // Single pass through online player infos - no need for triple iteration
        ref map<string, bool> sentAdmins = new map<string, bool>();
        ref array<ref QT_OnlinePlayerInfo> onlineInfos = new array<ref QT_OnlinePlayerInfo>();
        QT_QuestManager.GetInstance().GetOnlinePlayerInfos(onlineInfos);

        foreach (QT_OnlinePlayerInfo info : onlineInfos)
        {
            if (!info || !info.player || !info.player.GetIdentity()) continue;
            if (!IsQuestAdmin(info.player)) continue;
            string key = info.player.GetIdentity().GetId();
            if (sentAdmins.Contains(key)) continue;
            sentAdmins.Insert(key, true);
            SendAdminPlayerData(info.player);
        }
    }

    private static void AddAdminSnapshotInfoIfMissing(array<ref QT_AdminPlayerSnapshot> snapshots, QT_OnlinePlayerInfo info, QT_QuestManager mgr)
    {
        if (!snapshots || !info || info.uid == "" || !mgr) return;

        foreach (QT_AdminPlayerSnapshot existing : snapshots)
        {
            if (IsSameAdminSnapshot(existing, info.uid, info.steamId))
                return;
        }

        ref QT_AdminPlayerSnapshot snapshot = BuildAdminPlayerSnapshotFromInfo(info, mgr);
        if (snapshot) snapshots.Insert(snapshot);
    }

    private static void AddAdminSnapshotIfMissing(array<ref QT_AdminPlayerSnapshot> snapshots, PlayerBase pb, QT_QuestManager mgr)
    {
        if (!snapshots || !pb || !pb.GetIdentity() || !mgr) return;

        string uid = pb.GetIdentity().GetId();
        string steamId = pb.GetIdentity().GetPlainId();
        foreach (QT_AdminPlayerSnapshot existing : snapshots)
        {
            if (IsSameAdminSnapshot(existing, uid, steamId))
                return;
        }

        ref QT_AdminPlayerSnapshot snapshot = BuildAdminPlayerSnapshot(pb, mgr);
        if (snapshot) snapshots.Insert(snapshot);
    }

    private static bool IsSameAdminSnapshot(QT_AdminPlayerSnapshot existing, string uid, string steamId)
    {
        if (!existing) return false;

        if (uid != "" && existing.uid == uid)
            return true;

        if (steamId != "" && existing.steamId == steamId)
            return true;

        return false;
    }

    private static ref QT_AdminPlayerSnapshot BuildAdminPlayerSnapshotFromInfo(QT_OnlinePlayerInfo info, QT_QuestManager mgr)
    {
        if (!info || info.uid == "" || !mgr) return null;

        ref map<string, ref QT_PlayerQuestState> questMap = mgr.GetOrCreatePlayerMap(info.uid);
        ref QT_PlayerHistory hist = QT_QuestHistory.GetInstance().GetHistory(info.uid, info.name);

        ref QT_AdminPlayerSnapshot snapshot = new QT_AdminPlayerSnapshot();
        snapshot.uid = info.uid;
        snapshot.steamId = info.steamId;
        snapshot.name = info.name;
        snapshot.activeCount = CountAdminActiveQuests(questMap);
        snapshot.completedCount = CountAdminCompletedQuests(questMap, hist);
        snapshot.detailText = ClampAdminDetailText(BuildAdminPlayerSummaryFromValues(info.player, info.uid, info.name, info.steamId, questMap, hist));
        return snapshot;
    }

    private static ref QT_AdminPlayerSnapshot BuildAdminPlayerSnapshot(PlayerBase pb, QT_QuestManager mgr)
    {
        if (!pb || !pb.GetIdentity() || !mgr) return null;

        string uid = pb.GetIdentity().GetId();
        string steamId = pb.GetIdentity().GetPlainId();
        string name = pb.GetIdentity().GetName();
        ref map<string, ref QT_PlayerQuestState> questMap = mgr.GetOrCreatePlayerMap(uid);
        ref QT_PlayerHistory hist = QT_QuestHistory.GetInstance().GetHistory(uid, name);

        ref QT_AdminPlayerSnapshot snapshot = new QT_AdminPlayerSnapshot();
        snapshot.uid = uid;
        snapshot.steamId = steamId;
        snapshot.name = name;
        snapshot.activeCount = CountAdminActiveQuests(questMap);
        snapshot.completedCount = CountAdminCompletedQuests(questMap, hist);
        snapshot.detailText = ClampAdminDetailText(BuildAdminPlayerSummaryFromValues(pb, uid, name, steamId, questMap, hist));
        return snapshot;
    }

    private static int CountAdminActiveQuests(map<string, ref QT_PlayerQuestState> questMap)
    {
        int count = 0;
        if (!questMap) return count;

        foreach (string questId, QT_PlayerQuestState qs : questMap)
        {
            if (!qs) continue;
            if (qs.state == QT_QuestState.ACTIVE || qs.state == QT_QuestState.COMPLETED)
                count++;
        }
        return count;
    }

    private static int CountAdminCompletedQuests(map<string, ref QT_PlayerQuestState> questMap, QT_PlayerHistory hist)
    {
        int historyCount = 0;
        if (hist) historyCount = hist.totalQuestsCompleted;

        int dbCount = 0;
        if (questMap)
        {
            foreach (string questId, QT_PlayerQuestState qs : questMap)
            {
                if (!qs) continue;
                if (qs.state == QT_QuestState.TURNED_IN || qs.state == QT_QuestState.COOLDOWN)
                    dbCount++;
            }
        }

        return Math.Max(historyCount, dbCount);
    }

    private static string BuildAdminPlayerDetail(PlayerBase player, map<string, ref QT_PlayerQuestState> questMap, QT_PlayerHistory hist)
    {
        string uid = "";
        string steamId = "";
        string name = "";
        if (player && player.GetIdentity())
        {
            uid = player.GetIdentity().GetId();
            steamId = player.GetIdentity().GetPlainId();
            name = player.GetIdentity().GetName();
        }

        return BuildAdminPlayerSummaryFromValues(player, uid, name, steamId, questMap, hist);
    }

    private static string BuildAdminPlayerSummaryFromValues(PlayerBase player, string uid, string name, string steamId, map<string, ref QT_PlayerQuestState> questMap, QT_PlayerHistory hist)
    {
        string text = "";

        text = text + QT_L10nKey("ADMIN_NAME") + ": " + name + "\n";
        text = text + QT_L10nKey("ADMIN_CONNECTED") + ": " + QT_L10nKey("ADMIN_YES") + "\n";
        text = text + QT_L10nKey("ADMIN_UID") + ": " + SplitLongAdminValue(uid) + "\n";
        text = text + QT_L10nKey("ADMIN_STEAMID") + ": " + SplitLongAdminValue(steamId) + "\n";
        if (player)
        {
            vector playerPos = player.GetPosition();
            text = text + QT_L10nKey("ADMIN_POSITION") + ": ";
            text = text + playerPos[0].ToString();
            text = text + " ";
            text = text + playerPos[1].ToString();
            text = text + " ";
            text = text + playerPos[2].ToString();
            text = text + "\n";
        }
        text = text + QT_L10nKey("ADMIN_DB_FILE") + ": $profile:QuestTrader/players/" + uid + ".json\n";
        text = text + QT_L10nKey("ADMIN_HISTORY_FILE") + ": $profile:QuestTrader/history/" + uid + "_history.json\n\n";

        int savedCount = 0;
        if (questMap) savedCount = questMap.Count();
        int completedCount = 0;
        if (hist) completedCount = hist.totalQuestsCompleted;

        text = text + QT_L10nKey("ADMIN_SUMMARY") + "\n";
        text = text + "- " + QT_L10nKey("ADMIN_DB_SAVED") + ": " + savedCount.ToString() + "\n";
        text = text + "- " + QT_L10nKey("ADMIN_HISTORY_COMPLETED") + ": " + completedCount.ToString() + "\n\n";
        text = text + QT_L10nKey("ADMIN_HISTORY_ON_DEMAND") + "\n";

        return text;
    }

    private static string ClampAdminDetailText(string text)
    {
        if (text.Length() <= ADMIN_DETAIL_MAX_CHARS)
            return text;

        string output = text.Substring(0, ADMIN_DETAIL_MAX_CHARS);
        output = output + "\n...\n";
        output = output + QT_L10nKey("ADMIN_DETAILS_REDUCED");
        return output;
    }

    static void SendAdminHistoryPage(PlayerBase admin, string targetUID, int page)
    {
        if (!GetGame().IsServer()) return;
        if (!admin || !admin.GetIdentity()) return;
        if (IsServerRpcRateLimited(admin, "admin_history|" + targetUID, 300)) return;
        if (!IsQuestAdmin(admin))
        {
            SendToast(admin, QT_L10nKey("ADMIN_ACCESS_DENIED"), QT_ToastType.WARNING);
            return;
        }
        if (targetUID == "") return;

        ref QT_PlayerHistory hist = QT_QuestHistory.GetInstance().GetHistory(targetUID, "");
        int totalEntries = 0;
        if (hist && hist.entries) totalEntries = hist.entries.Count();

        int totalPages = 1;
        if (totalEntries > 0)
            totalPages = (totalEntries + ADMIN_HISTORY_PAGE_SIZE - 1) / ADMIN_HISTORY_PAGE_SIZE;

        if (page < 0) page = 0;
        if (page >= totalPages) page = totalPages - 1;

        ref array<string> lines = new array<string>();
        if (hist && hist.entries && totalEntries > 0)
        {
            int newestIndex = totalEntries - 1 - (page * ADMIN_HISTORY_PAGE_SIZE);
            int endIndex = newestIndex - ADMIN_HISTORY_PAGE_SIZE + 1;
            if (endIndex < 0) endIndex = 0;

            for (int i = newestIndex; i >= endIndex; i--)
            {
                QT_HistoryEntry e = hist.entries[i];
                if (!e) continue;
                lines.Insert(BuildAdminHistoryLine(e));
            }
        }

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(targetUID);
        rpc.Write(page);
        rpc.Write(totalPages);
        rpc.Write(totalEntries);
        rpc.Write(lines.Count());
        foreach (string line : lines)
            rpc.Write(line);
        QT_Perf.Log("rpc send type=ADMIN_HISTORY_PAGE admin=" + admin.GetIdentity().GetName() + " items=" + lines.Count().ToString() + " totalHistory=" + totalEntries.ToString() + " approxPayloadBytes=" + (lines.Count() * ADMIN_HISTORY_LINE_MAX_CHARS).ToString());
        rpc.Send(admin, QT_RPC.ADMIN_HISTORY_PAGE, true, admin.GetIdentity());
    }

    private static string BuildAdminHistoryLine(QT_HistoryEntry e)
    {
        if (!e) return "";

        string line = e.questId;
        if (line == "") line = "quest";
        line = line + " | Completed | ";
        if (e.completedAt != "")
            line = line + e.completedAt;
        else
            line = line + e.completedEpoch.ToString();

        if (e.runNumber > 1)
            line = line + " | run #" + e.runNumber.ToString();

        if (line.Length() > ADMIN_HISTORY_LINE_MAX_CHARS)
            line = line.Substring(0, ADMIN_HISTORY_LINE_MAX_CHARS - 3) + "...";

        return line;
    }

    private static string SplitLongAdminValue(string value)
    {
        if (value.Length() <= 28) return value;

        string output = "";
        string remaining = value;
        while (remaining.Length() > 28)
        {
            if (output != "") output = output + "\n     ";
            output = output + remaining.Substring(0, 28);
            remaining = remaining.Substring(28, remaining.Length() - 28);
        }
        if (remaining != "")
        {
            if (output != "") output = output + "\n     ";
            output = output + remaining;
        }
        return output;
    }

    private static string AdminStateName(int state)
    {
        if (state == QT_QuestState.AVAILABLE) return QT_L10nKey("STATE_AVAILABLE");
        if (state == QT_QuestState.ACTIVE) return QT_L10nKey("STATE_ACTIVE");
        if (state == QT_QuestState.COMPLETED) return QT_L10nKey("STATE_READY");
        if (state == QT_QuestState.TURNED_IN) return QT_L10nKey("STATE_COMPLETED");
        if (state == QT_QuestState.COOLDOWN) return QT_L10nKey("STATE_COOLDOWN");
        return QT_L10nKey("ADMIN_UNKNOWN");
    }

    private static string BuildAdminProgressText(QT_PlayerQuestState qs, QT_QuestDef def)
    {
        if (!qs) return "-";
        if (!def || !def.objectives || def.objectives.Count() == 0)
            return QT_L10nKey("ADMIN_NO_OBJECTIVES");

        string text = "";
        for (int i = 0; i < def.objectives.Count(); i++)
        {
            QT_Objective obj = def.objectives[i];
            int cur = 0;
            if (i < qs.objectiveProgress.Count()) cur = qs.objectiveProgress[i];
            if (text != "") text = text + ", ";
            text = text + cur.ToString();
            text = text + "/";
            text = text + obj.requiredAmount.ToString();
        }
        if (text == "") text = "-";
        return text;
    }

    static void SendAdminLog(PlayerBase admin)
    {
        if (!GetGame().IsServer()) return;
        if (!IsQuestAdmin(admin))
        {
            SendToast(admin, QT_L10nKey("ADMIN_ACCESS_DENIED"), QT_ToastType.WARNING);
            return;
        }
        array<string> lines = QT_Logger.GetInstance().GetRecentLines(50);
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(lines.Count());
        foreach (string line : lines) rpc.Write(line);
        rpc.Send(admin, QT_RPC.ADMIN_LOG_DATA, true, admin.GetIdentity());
    }

    private static string BuildAdminObjectivesText(QT_QuestDef def)
    {
        string text = "";
        if (!def || !def.objectives) return text;
        foreach (QT_Objective obj : def.objectives)
        {
            if (text != "") text = text + ";";
            string itemClass = obj.itemClassName;
            string entityClass = obj.entityClassName;
            text = text + itemClass;
            text = text + "|";
            text = text + obj.requiredAmount.ToString();
            text = text + "|";
            text = text + obj.description;
            text = text + "|";
            text = text + entityClass;
        }
        return text;
    }

    private static string BuildAdminRewardsText(QT_QuestDef def)
    {
        string text = "";
        if (!def || !def.rewards) return text;
        foreach (QT_Reward rew : def.rewards)
        {
            if (text != "") text = text + ";";
            text = text + rew.itemClassName + "|" + rew.amount.ToString();
        }
        return text;
    }

    private static string JoinStringArray(array<string> values, string separator)
    {
        string text = "";
        if (!values) return text;
        foreach (string value : values)
        {
            if (text != "") text = text + separator;
            text = text + value;
        }
        return text;
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

    static void RequestReconComplete(string questId)
    {
        if (!GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(questId);
        rpc.Send(null, QT_RPC.RECON_COMPLETE, true);
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

    static void RequestObjectInteract(string questId, string objectClassName, vector objectPosition)
    {
        if (!GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(questId);
        rpc.Write(objectClassName);
        rpc.Write(objectPosition);
        rpc.Send(null, QT_RPC.OBJECT_INTERACT, true);
    }

    static void RequestQuestLog()
    {
        if (!GetGame().IsClient()) return;
        int now = GetGame().GetTime();
        if (now < s_nextQuestLogRequestTime) return;
        s_nextQuestLogRequestTime = now + 700;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(0);
        rpc.Send(null, QT_RPC.QUEST_LOG, true);
    }

    static void RequestJournal()
    {
        if (!GetGame().IsClient()) return;
        int now = GetGame().GetTime();
        if (now < s_nextJournalRequestTime) return;
        s_nextJournalRequestTime = now + 700;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(0);
        rpc.Send(null, QT_RPC.REQUEST_JOURNAL, true);
    }

    static void RequestTraderPositions()
    {
        if (!GetGame().IsClient()) return;
        int now = GetGame().GetTime();
        if (now < s_nextPositionsRequestTime) return;
        s_nextPositionsRequestTime = now + 1500;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(0);
        rpc.Send(null, QT_RPC.REQUEST_POSITIONS, true);
    }

    static void RequestAdminData()
    {
        if (!GetGame().IsClient()) return;
        int now = GetGame().GetTime();
        if (now < s_nextAdminDataRequestTime) return;
        s_nextAdminDataRequestTime = now + 800;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(0);
        rpc.Send(null, QT_RPC.REQUEST_ADMIN_DATA, true);
    }

    static void RequestAdminLog()
    {
        if (!GetGame().IsClient()) return;
        int now = GetGame().GetTime();
        if (now < s_nextAdminLogRequestTime) return;
        s_nextAdminLogRequestTime = now + 800;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(0);
        rpc.Send(null, QT_RPC.REQUEST_ADMIN_LOG, true);
    }

    static void RequestAdminHistoryPage(string uid, int page)
    {
        if (!GetGame().IsClient()) return;
        if (uid == "") return;
        int now = GetGame().GetTime();
        if (now < s_nextAdminHistoryRequestTime) return;
        s_nextAdminHistoryRequestTime = now + 100;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(uid);
        rpc.Write(page);
        rpc.Send(null, QT_RPC.ADMIN_HISTORY_PAGE, true);
    }

    static void SendAdminCommand(string cmd, string targetUID, string param)
    {
        if (!GetGame().IsClient()) return;
        int now = GetGame().GetTime();
        if (now < s_nextAdminCommandTime) return;
        s_nextAdminCommandTime = now + 500;
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
        if (!AreQuestChatMessagesEnabled()) return;

        GetGame().RPCSingleParam(player, ERPCs.RPC_USER_ACTION_MESSAGE, new Param1<string>(msg), true, player.GetIdentity());
    }

    private static bool AreQuestChatMessagesEnabled()
    {
        QT_QuestManager mgr = QT_QuestManager.GetInstance();
        if (!mgr) return true;

        QT_Config cfg = mgr.GetConfig();
        if (!cfg || !cfg.Settings) return true;

        return cfg.Settings.EnableQuestChatMessages;
    }

    // Send quest objectives info to player chat — called on accept so player
    // can see what they need to do without reopening the menu
    static void SendQuestInfo(PlayerBase player, QT_QuestDef def)
    {
        if (!player || !def) return;
        if (!AreQuestChatMessagesEnabled()) return;

        QT_QuestManager mgr = QT_QuestManager.GetInstance();
        QT_Config cfg = mgr.GetConfig();

        SendChatLine(player, "#QuestTrader_CHAT_QUEST_ACCEPTED: " + def.title);

        string details = "";
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
            details = "#QuestTrader_CHAT_DELIVER_TO: " + destName;
        }
        else if (def.type == QT_QuestType.INTERACT)
        {
            details = "[ ] " + BuildInteractionObjectiveText(def) + " (0/1)";
            string turnInNameInteract = def.traderId;
            if (cfg)
            {
                foreach (QT_TraderDef tdInteract : cfg.TraderNPCPositions)
                {
                    if (tdInteract.id == def.traderId) { turnInNameInteract = tdInteract.name; break; }
                }
            }
            details = details + " | #QuestTrader_CHAT_TURN_IN_TO: " + turnInNameInteract;
        }
        else
        {
            foreach (QT_Objective obj : def.objectives)
            {
                if (details != "") details = details + " | ";
                details = details + "[ ] " + BuildObjectiveDisplayText(def, obj) + " (0/" + obj.requiredAmount + ")";
            }
            string turnInName = def.traderId;
            if (cfg)
            {
                foreach (QT_TraderDef td2 : cfg.TraderNPCPositions)
                {
                    if (td2.id == def.traderId) { turnInName = td2.name; break; }
                }
            }
            if (details != "") details = details + " | ";
            details = details + "#QuestTrader_CHAT_TURN_IN_TO: " + turnInName;
        }

        if (details != "")
            SendChatLine(player, details);
    }

    // S->C: tell client a trader is nearby
    static void SendTraderPositions(PlayerBase player)
    {
        if (!GetGame().IsServer()) return;
        if (!player || !player.GetIdentity()) return;
        if (IsServerRpcRateLimited(player, "trader_positions", 1000)) return;
        QT_Config cfg = QT_QuestManager.GetInstance().GetConfig();
        if (!cfg) return;
        ScriptRPC rpc = new ScriptRPC();
        int traderCount = cfg.TraderNPCPositions.Count();
        if (traderCount > MAX_TRADER_POSITIONS_PER_PACKET) traderCount = MAX_TRADER_POSITIONS_PER_PACKET;
        rpc.Write(traderCount);
        for (int traderIdx = 0; traderIdx < traderCount; traderIdx++)
        {
            QT_TraderDef def = cfg.TraderNPCPositions[traderIdx];
            rpc.Write(def.id);
            vector pos = def.position;
            pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
            rpc.Write(pos);
            rpc.Write(cfg.Settings.interactionDistance);
            rpc.Write(def.name);
            // Pick a random greeting to send with positions
            string greeting = def.greeting;
            if (def.greetings && def.greetings.Count() > 0)
                greeting = def.greetings[Math.RandomInt(0, def.greetings.Count())];
            rpc.Write(greeting);
        }
        QT_Perf.Log("rpc send type=TRADER_POSITIONS player=" + player.GetIdentity().GetName() + " items=" + traderCount.ToString() + " approxPayloadBytes=" + (traderCount * 96).ToString());
        rpc.Send(player, QT_RPC.TRADER_POSITIONS, true, player.GetIdentity());
    }

    // C->S: player pressed F near a trader
    static void RequestInteractTrader(string traderId)
    {
        if (!GetGame().IsClient()) return;
        int now = GetGame().GetTime();
        if (now < s_nextInteractRequestTime) return;
        s_nextInteractRequestTime = now + 300;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(traderId);
        rpc.Send(null, QT_RPC.INTERACT_TRADER, true);
    }

    private static void RefreshTraderBoard(PlayerBase player, string traderId)
    {
        if (!player || traderId == "") return;
        SendQuestList(player, traderId, false);
    }

    static void HandleRPC(PlayerBase player, int rpcType, ParamsReadContext ctx)
    {
        if (!player || !ctx) return;
        if (!QT_RPCGuard.IsQuestTraderRPC(rpcType)) return;

        if (rpcType == QT_RPC.ACCEPT_QUEST)
        {
            string questId;
            if (!ctx.Read(questId)) return;
            if (!IsSafeRpcString(questId, 128)) return;
            QT_QuestManager mgrAccept = QT_QuestManager.GetInstance();
            QT_QuestDef defAccept = mgrAccept.GetQuestDef(questId);
            if (mgrAccept.AcceptQuest(player, questId) && defAccept)
                RefreshTraderBoard(player, defAccept.traderId);
        }
        else if (rpcType == QT_RPC.CANCEL_QUEST)
        {
            string cancelId;
            if (!ctx.Read(cancelId)) return;
            if (!IsSafeRpcString(cancelId, 128)) return;
            QT_QuestManager mgrCancel = QT_QuestManager.GetInstance();
            QT_QuestDef defCancel = mgrCancel.GetQuestDef(cancelId);
            if (mgrCancel.CancelQuest(player, cancelId) && defCancel)
                RefreshTraderBoard(player, defCancel.traderId);
        }
        else if (rpcType == QT_RPC.RECON_COMPLETE)
        {
            string reconQuestId;
            if (!ctx.Read(reconQuestId)) return;
            if (!IsSafeRpcString(reconQuestId, 128)) return;
            QT_QuestManager.GetInstance().CompleteReconObjective(player, reconQuestId);
        }
        else if (rpcType == QT_RPC.TURN_IN_QUEST)
        {
            string questId2;
            string turnInTraderId;
            if (!ctx.Read(questId2)) return;
            if (!ctx.Read(turnInTraderId)) return;
            if (!IsSafeRpcString(questId2, 128)) return;
            if (!IsSafeRpcString(turnInTraderId, 128)) return;
            QT_QuestManager mgrTurnIn = QT_QuestManager.GetInstance();
            QT_QuestDef def = mgrTurnIn.GetQuestDef(questId2);
            if (!def) return;
            bool turnInDone = false;
            if (def.type == QT_QuestType.COLLECT)
                turnInDone = mgrTurnIn.TurnInCollectQuest(player, questId2);
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
                    QT_RPCManager.SendToast(player, QT_L10nKey("TOAST_DELIVER_THIS_TO") + " " + deliveryName, QT_ToastType.WARNING);
                    return;
                }
                turnInDone = mgrTurnIn.TurnInDeliverQuest(player, questId2);
            }
            else if (def.type == QT_QuestType.INTERACT)
                turnInDone = mgrTurnIn.TurnInInteractionQuest(player, questId2);
            else
                turnInDone = mgrTurnIn.TurnInKillQuest(player, questId2);

            if (turnInDone)
                RefreshTraderBoard(player, turnInTraderId);
        }
        else if (rpcType == QT_RPC.OBJECT_INTERACT)
        {
            string objectQuestId;
            string objectClassName;
            vector objectPosition;
            if (!ctx.Read(objectQuestId)) return;
            if (!ctx.Read(objectClassName)) return;
            if (!ctx.Read(objectPosition)) return;
            if (!IsSafeRpcString(objectQuestId, 128)) return;
            if (!IsSafeRpcString(objectClassName, 128)) return;
            QT_QuestManager.GetInstance().CompleteInteractionObjective(player, objectQuestId, objectClassName, objectPosition);
        }
        else if (rpcType == QT_RPC.QUEST_LOG)
        {
            if (IsClientRequestRateLimited(player, "quest_log", 2000)) return;
            SendQuestLog(player);
        }
        else if (rpcType == QT_RPC.REQUEST_JOURNAL)
        {
            if (IsClientRequestRateLimited(player, "journal", 1500)) return;
            SendJournal(player);
        }
        else if (rpcType == QT_RPC.REQUEST_ADMIN_DATA)
        {
            if (IsClientRequestRateLimited(player, "admin_data", 1500)) return;
            if (!IsQuestAdmin(player))
            {
                SendToast(player, QT_L10nKey("ADMIN_ACCESS_DENIED"), QT_ToastType.WARNING);
                return;
            }
            SendAdminPlayerData(player);
        }
        else if (rpcType == QT_RPC.REQUEST_ADMIN_LOG)
        {
            if (IsClientRequestRateLimited(player, "admin_log", 1500)) return;
            if (!IsQuestAdmin(player))
            {
                SendToast(player, QT_L10nKey("ADMIN_ACCESS_DENIED"), QT_ToastType.WARNING);
                return;
            }
            SendAdminLog(player);
        }
        else if (rpcType == QT_RPC.ADMIN_HISTORY_PAGE)
        {
            if (!IsQuestAdmin(player))
            {
                SendToast(player, QT_L10nKey("ADMIN_ACCESS_DENIED"), QT_ToastType.WARNING);
                return;
            }
            string historyUID;
            int historyPage;
            if (!ctx.Read(historyUID)) return;
            if (!ctx.Read(historyPage)) return;
            if (!IsSafeRpcString(historyUID, 128)) return;
            if (historyPage < 0 || historyPage > 10000) return;
            if (IsClientRequestRateLimited(player, "admin_history", 500)) return;
            SendAdminHistoryPage(player, historyUID, historyPage);
        }
        else if (rpcType == QT_RPC.INTERACT_TRADER)
        {
            string interactId;
            if (!ctx.Read(interactId)) return;
            if (!IsSafeRpcString(interactId, 128)) return;
            if (interactId == "") return;
            if (IsClientRequestRateLimited(player, "interact_trader|" + interactId, 750)) return;
            SendQuestList(player, interactId);
        }
        else if (rpcType == QT_RPC.ADMIN_COMMAND)
        {
            DispatchAdminCommand(player, ctx);
        }
        else if (rpcType == QT_RPC.REQUEST_POSITIONS)
        {
            if (IsClientRequestRateLimited(player, "positions", 5000)) return;
            SendTraderPositions(player);
        }
    }

    private static void DispatchAdminCommand(PlayerBase admin, ParamsReadContext ctx)
    {
        string cmd, targetUID, param;
        if (!ctx.Read(cmd))       return;
        if (!ctx.Read(targetUID)) return;
        if (!ctx.Read(param))     return;
        if (!IsSafeRpcString(cmd, 64)) return;
        if (!IsSafeRpcString(targetUID, 128)) return;
        if (!IsSafeRpcString(param, 256)) return;

        if (!IsQuestAdmin(admin))
        {
            SendToast(admin, QT_L10nKey("ADMIN_ACCESS_DENIED"), QT_ToastType.WARNING);
            string blockedUID = "";
            string blockedName = "";
            if (admin && admin.GetIdentity())
            {
                blockedUID = admin.GetIdentity().GetId();
                blockedName = admin.GetIdentity().GetName();
            }
            QT_Logger.GetInstance().Warn("ADMIN", "Blocked admin command '" + cmd + "'", blockedUID, blockedName);
            return;
        }

        QT_QuestManager mgr = QT_QuestManager.GetInstance();
        string adminName = admin.GetIdentity().GetName();
        string adminUID  = admin.GetIdentity().GetId();
        string trimmedParam = param.Trim();

        if (IsAdminCommandBlocked(adminUID, cmd, targetUID, trimmedParam))
        {
            SendToast(admin, QT_L10nKey("ADMIN_WAIT_REPEAT"), QT_ToastType.WARNING);
            return;
        }

        if (cmd == "RESET_QUEST")
        {
            if (targetUID == "")
            {
                SendToast(admin, QT_L10nKey("ADMIN_SELECT_PLAYER"), QT_ToastType.WARNING);
                return;
            }
            BlockAdminCommand(adminUID, cmd, targetUID, trimmedParam, 1200);
            mgr.AdminResetQuest(targetUID, param);
            if (trimmedParam == "")
                QT_Logger.GetInstance().Info("ADMIN", "Reset all quest states for " + targetUID, adminUID, adminName);
            else
                QT_Logger.GetInstance().Info("ADMIN", "Reset quest '" + param + "' for " + targetUID, adminUID, adminName);
            SendAdminPlayerData(admin);
            if (trimmedParam == "")
                SendToast(admin, QT_L10nKey("ADMIN_ALL_RESET"), QT_ToastType.INFO);
            else
                SendToast(admin, QT_L10nKey("ADMIN_QUEST_RESET"), QT_ToastType.INFO);
        }
        else if (cmd == "COMPLETE_QUEST")
        {
            if (targetUID == "")
            {
                SendToast(admin, QT_L10nKey("ADMIN_SELECT_PLAYER"), QT_ToastType.WARNING);
                return;
            }
            BlockAdminCommand(adminUID, cmd, targetUID, trimmedParam, 1200);
            bool completeOk = mgr.AdminCompleteQuest(targetUID, param);

            string completeLog = "Force-completed quest";
            completeLog = completeLog + " (input: '";
            completeLog = completeLog + param;
            completeLog = completeLog + "') for ";
            completeLog = completeLog + targetUID;
            if (completeOk)
                QT_Logger.GetInstance().Info("ADMIN", completeLog, adminUID, adminName);
            else
                QT_Logger.GetInstance().Warn("ADMIN", "Failed to " + completeLog, adminUID, adminName);

            SendAdminPlayerData(admin);
            if (completeOk)
                SendToast(admin, QT_L10nKey("ADMIN_QUEST_COMPLETED_PLAYER"), QT_ToastType.INFO);
            else
                SendToast(admin, QT_L10nKey("ADMIN_INVALID_QUEST"), QT_ToastType.WARNING);
        }
        else if (cmd == "WIPE_PLAYER")
        {
            if (targetUID == "")
            {
                SendToast(admin, QT_L10nKey("ADMIN_SELECT_PLAYER"), QT_ToastType.WARNING);
                return;
            }
            BlockAdminCommand(adminUID, cmd, targetUID, "", 1500);
            mgr.AdminWipePlayer(targetUID);
            QT_Logger.GetInstance().Info("ADMIN", "Wiped quests for " + targetUID, adminUID, adminName);
            SendAdminPlayerData(admin);
            SendToast(admin, QT_L10nKey("ADMIN_PLAYER_WIPED"), QT_ToastType.WARNING);
        }
        else if (cmd == "RELOAD_CONFIG")
        {
            BlockAdminCommand(adminUID, cmd, "", "", 2500);
            QT_Logger.GetInstance().Info("ADMIN", "Config reload by " + adminName);
            bool reloadOk = mgr.ReloadConfig();
            if (reloadOk)
            {
                QT_TraderSpawner.GetActiveSpawner().RespawnAllChunked(mgr.GetConfig());
                QueuePlayerRefresh();
                SendAdminPlayerData(admin);
                SendToast(admin, QT_L10nKey("ADMIN_CONFIG_RELOADED"), QT_ToastType.INFO);
            }
            else
            {
                SendToast(admin, QT_L10nKey("ADMIN_CONFIG_RELOAD_FAILED"), QT_ToastType.WARNING);
            }
        }
        else if (cmd == "RESPAWN_NPCS")
        {
            BlockAdminCommand(adminUID, cmd, "", "", 2500);
            QT_Logger.GetInstance().Info("ADMIN", "NPC respawn by " + adminName);
            QT_TraderSpawner.GetActiveSpawner().RespawnAllChunked(mgr.GetConfig());
            QueuePlayerRefresh();
            SendToast(admin, QT_L10nKey("ADMIN_NPC_RESPAWN_STARTED"), QT_ToastType.INFO);
        }
        else
        {
            QT_Logger.GetInstance().Warn("ADMIN", "Unknown admin command '" + cmd + "'", adminUID, adminName);
            SendToast(admin, QT_L10nKey("ADMIN_ACCESS_DENIED"), QT_ToastType.WARNING);
        }
    }

    private static bool IsAdminCommandBlocked(string adminUID, string cmd, string targetUID, string param)
    {
        if (!s_adminCommandBlockUntil)
            s_adminCommandBlockUntil = new map<string, int>();

        PurgeExpiredAdminCommandBlocks();

        string key = BuildAdminCommandKey(adminUID, cmd, targetUID, param);
        if (!s_adminCommandBlockUntil.Contains(key))
            return false;

        int now = GetGame().GetTime();
        int until = s_adminCommandBlockUntil.Get(key);
        if (now < until)
            return true;

        s_adminCommandBlockUntil.Remove(key);
        return false;
    }

    private static void PurgeExpiredAdminCommandBlocks()
    {
        if (!s_adminCommandBlockUntil) return;

        int now = GetGame().GetTime();
        ref array<string> expiredKeys = new array<string>();
        foreach (string key, int until : s_adminCommandBlockUntil)
        {
            if (now >= until)
                expiredKeys.Insert(key);
        }

        foreach (string expiredKey : expiredKeys)
        {
            s_adminCommandBlockUntil.Remove(expiredKey);
        }
    }

    private static void BlockAdminCommand(string adminUID, string cmd, string targetUID, string param, int durationMs)
    {
        if (!s_adminCommandBlockUntil)
            s_adminCommandBlockUntil = new map<string, int>();

        string key = BuildAdminCommandKey(adminUID, cmd, targetUID, param);
        s_adminCommandBlockUntil.Set(key, GetGame().GetTime() + durationMs);
    }

    private static string BuildAdminCommandKey(string adminUID, string cmd, string targetUID, string param)
    {
        return adminUID + "|" + cmd + "|" + targetUID + "|" + param;
    }

    static bool IsQuestAdmin(PlayerBase player)
    {
        if (!player || !player.GetIdentity()) return false;

        if (IsQuestAdminUID(player.GetIdentity().GetId()))
            return true;

        string plainId = player.GetIdentity().GetPlainId();
        if (plainId != "" && IsQuestAdminUID(plainId))
            return true;

        return false;
    }

    static bool IsQuestAdminUID(string uid)
    {
        uid = uid.Trim();
        if (uid == "") return false;

        QT_Config cfg = QT_QuestManager.GetInstance().GetConfig();
        if (cfg && cfg.AdminSteamIds)
        {
            foreach (string adminId : cfg.AdminSteamIds)
            {
                if (adminId.Trim() == uid) return true;
            }
        }

        if (IsAdminUIDInQuestConfig("$profile:QuestTrader/QuestConfig.json", uid)) return true;
        if (IsAdminUIDInQuestConfig("QuestTrader/config/QuestConfig.json", uid)) return true;
        return false;
    }

    private static bool IsAdminUIDInQuestConfig(string path, string uid)
    {
        if (!FileExist(path)) return false;

        string json = QT_JsonHelper.ReadFileToString(path);
        if (json == "") return false;

        ref QT_Config cfg;
        string err;
        JsonSerializer ser = new JsonSerializer();
        if (!ser.ReadFromString(cfg, json, err)) return false;
        if (!cfg || !cfg.AdminSteamIds) return false;

        foreach (string adminId : cfg.AdminSteamIds)
        {
            if (adminId.Trim() == uid) return true;
        }
        return false;
    }

    static string GetDisplayName(string className)
    {
        if (className == "") return "";

        string displayName = "";
        GetGame().ConfigGetText("CfgVehicles " + className + " displayName", displayName);
        if (displayName != "") return displayName;

        GetGame().ConfigGetText("CfgWeapons " + className + " displayName", displayName);
        if (displayName != "") return displayName;

        GetGame().ConfigGetText("CfgMagazines " + className + " displayName", displayName);
        if (displayName != "") return displayName;

        return className;
    }

    static string BuildObjectiveDisplayText(QT_QuestDef def, QT_Objective obj)
    {
        if (!def || !obj) return "";

        if (obj.itemClassName != "")
        {
            string itemName = GetDisplayName(obj.itemClassName);
            if (def.type == QT_QuestType.DELIVER)
                return QT_L10nKey("OBJECTIVE_DELIVER") + " " + obj.requiredAmount.ToString() + "x " + itemName;
            return QT_L10nKey("OBJECTIVE_COLLECT") + " " + obj.requiredAmount.ToString() + "x " + itemName;
        }

        if (def.type == QT_QuestType.KILL && obj.description != "")
            return obj.description;

        if (def.type == QT_QuestType.INTERACT)
        {
            if (obj.description != "") return obj.description;
            return BuildInteractionObjectiveText(def);
        }

        if (obj.entityClassName != "")
            return obj.description;

        return obj.description;
    }

    static string BuildObjectiveTokenText(QT_QuestDef def, QT_Objective obj)
    {
        if (!def || !obj) return "";

        if (obj.itemClassName != "")
        {
            if (def.type == QT_QuestType.DELIVER)
                return QT_L10nKey("OBJECTIVE_DELIVER") + " " + obj.requiredAmount.ToString() + "x " + obj.itemClassName;
            return QT_L10nKey("OBJECTIVE_COLLECT") + " " + obj.requiredAmount.ToString() + "x " + obj.itemClassName;
        }

        if (def.type == QT_QuestType.KILL && obj.entityClassName == "QT_ReconObjective")
            return QT_L10nKey("OBJECTIVE_RECON");

        if (def.type == QT_QuestType.KILL && obj.entityClassName != "")
            return QT_L10nKey("OBJECTIVE_KILL") + " " + obj.requiredAmount.ToString() + "x " + obj.entityClassName;

        if (def.type == QT_QuestType.KILL && obj.Targets && obj.Targets.Count() > 0)
            return QT_L10nKey("OBJECTIVE_KILL") + " " + obj.requiredAmount.ToString() + "x " + obj.Targets[0];

        if (def.type == QT_QuestType.INTERACT)
        {
            if (obj.description != "") return obj.description;
            return BuildInteractionObjectiveText(def);
        }

        if (obj.entityClassName != "")
            return obj.description;

        return obj.description;
    }

    private static string QT_L10nKey(string id)
    {
        return "#QuestTrader_" + id;
    }

    private static string BuildInteractionObjectiveText(QT_QuestDef def)
    {
        if (!def) return QT_L10nKey("OBJECTIVE_INTERACT");

        string target = def.interactionObjectClassName;
        if (target == "" && def.interactionPosition != vector.Zero)
            target = def.interactionPosition.ToString();
        if (target == "") return QT_L10nKey("OBJECTIVE_INTERACT");

        return QT_L10nKey("OBJECTIVE_INTERACT") + " " + target;
    }
}
