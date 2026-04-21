// ============================================================
//  QuestTrader | QT_MissionHooks.c  (5_Mission)
//
//  MissionServer valid overrides (confirmed from source):
//    OnInit, OnUpdate, OnMissionStart, OnMissionFinish, OnEvent,
//    OnGameplayDataHandlerLoad, IsServer
//
//  MissionGameplay valid overrides:
//    OnInit, OnUpdate
//
//  OnRPC -> moved to DayZGame (3_Game/QT_DayZGameHook.c)
//  EntityKilled -> moved to EntityAI.EEKilled (4_World/QT_EntityAIHook.c)
// ============================================================

// ============================================================
//  SERVER
// ============================================================
modded class MissionServer
{
    private ref QT_TraderSpawner m_traderSpawner;
    private float m_autosaveTimer = 0;
    private static const float AUTOSAVE_INTERVAL = 60.0;

    override void OnInit()
    {
        super.OnInit();
        if (!GetGame().IsServer()) return;
        QT_Logger.GetInstance();
        QT_QuestHistory.GetInstance();
        QT_QuestManager.GetInstance();
        QT_TraderMarker.GetInstance();
        Print("[QuestTrader] MissionServer OnInit complete.");
    }

    override void OnMissionStart()
    {
        super.OnMissionStart();
        if (!GetGame().IsServer()) return;

        // Spawn NPCs here - world is fully loaded
        m_traderSpawner = new QT_TraderSpawner();
        m_traderSpawner.SpawnAll(QT_QuestManager.GetInstance().GetConfig());
        QT_Logger.GetInstance().Info("SYSTEM", "QuestTrader NPCs spawned.");
        Print("[QuestTrader] OnMissionStart - spawning traders.");
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (!GetGame().IsServer()) return;

        m_autosaveTimer += timeslice;
        if (m_autosaveTimer >= AUTOSAVE_INTERVAL)
        {
            m_autosaveTimer = 0;
            QT_QuestManager.GetInstance().SaveAll();
        }

        // Check traders are alive every 30s and respawn if dead
        if (m_traderSpawner) m_traderSpawner.CheckAndRespawn(timeslice);
    }

    override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
    {
        super.InvokeOnConnect(player, identity);
        if (!GetGame().IsServer() || !player || !identity) return;

        // Push trader positions directly from mission level as a reliable fallback.
        // OnScheduledTick in QT_PlayerBase can be interrupted by !!! OnError() on connect.
        Print("[QuestTrader] InvokeOnConnect - pushing trader positions to: " + identity.GetName());
        QT_RPCManager.SendTraderPositions(player);
    }

    override void OnMissionFinish()
    {
        super.OnMissionFinish();
        if (GetGame().IsServer())
        {
            QT_QuestManager.GetInstance().SaveAll();
            QT_TraderMarker.GetInstance().DeleteMarker();
            QT_Logger.GetInstance().Info("SYSTEM", "Mission finished - final save done.");
        }
    }

    override void OnEvent(EventType eventTypeId, Param params)
    {
        super.OnEvent(eventTypeId, params);

        // Intercept chat messages for /q command
        if (eventTypeId == ChatMessageEventTypeID)
        {
            ChatMessageEventParams chatParams;
            if (!Class.CastTo(chatParams, params)) return;
            if (!chatParams) return;

            // ChatMessageEventParams: param1=channel, param2=senderName, param3=text, param4=to
            string msg = chatParams.param3;
            // Match "/q" or "q" as the last word in the message.
            // GlobalChatPlus prepends a channel prefix, so we check the final word only.
            // GlobalChatPlus prepends channel prefix so check last word only
            string msgTrimmed = msg.Trim();
            string lastWord = msgTrimmed;
            for (int ci = msgTrimmed.Length() - 1; ci >= 0; ci--)
            {
                if (msgTrimmed.Get(ci) == " ")
                {
                    lastWord = msgTrimmed.Substring(ci + 1, msgTrimmed.Length() - ci - 1);
                    break;
                }
            }
            if (lastWord != "/q" && lastWord != "q") return;

            string senderName = chatParams.param2;
            array<Man> players = new array<Man>();
            GetGame().GetPlayers(players);
            foreach (Man man : players)
            {
                PlayerBase player = PlayerBase.Cast(man);
                if (!player || !player.GetIdentity()) continue;
                if (player.GetIdentity().GetName() != senderName) continue;

                string uid = player.GetIdentity().GetId();
                QT_QuestManager mgr = QT_QuestManager.GetInstance();
                auto playerMap = mgr.GetOrCreatePlayerMap(uid);

                bool any = false;
                foreach (string questId, ref QT_PlayerQuestState qs : playerMap)
                {
                    if (qs.state != QT_QuestState.ACTIVE && qs.state != QT_QuestState.COMPLETED) continue;
                    QT_QuestDef def = mgr.GetQuestDef(questId);
                    if (!def) continue;

                    string stateStr = "Active";
                    if (qs.state == QT_QuestState.COMPLETED) stateStr = "READY TO TURN IN";

                    QT_RPCManager.SendChatLine(player, "[Quest] " + def.title + " - " + stateStr);

                    // Look up the trader name for turn-in info
                    QT_Config cfg = mgr.GetConfig();
                    if (def.type == QT_QuestType.DELIVER)
                    {
                        // Deliver quests: show destination trader
                        string destName = def.deliveryTraderId;
                        if (cfg)
                        {
                            foreach (QT_TraderDef td : cfg.TraderNPCPositions)
                            {
                                if (td.id == def.deliveryTraderId)
                                {
                                    destName = td.name;
                                    break;
                                }
                            }
                        }
                        QT_RPCManager.SendChatLine(player, "  Deliver to: " + destName);
                    }
                    else
                    {
                        // Collect/Kill quests: show objectives then turn-in trader
                        for (int o = 0; o < def.objectives.Count(); o++)
                        {
                            QT_Objective obj = def.objectives[o];
                            int prog = 0;
                            if (def.type == QT_QuestType.COLLECT)
                            {
                                // Count items live from inventory - same as quest menu
                                prog = mgr.CountItemsInInventory(player, obj.itemClassName);
                                if (prog > obj.requiredAmount) prog = obj.requiredAmount;
                            }
                            else if (o < qs.objectiveProgress.Count())
                            {
                                prog = qs.objectiveProgress[o];
                            }
                            string tick = "[ ] ";
                            if (prog >= obj.requiredAmount) tick = "[x] ";
                            string objLine = "  " + tick + obj.description + " (" + prog + "/" + obj.requiredAmount + ")";
                            QT_RPCManager.SendChatLine(player, objLine);
                        }
                        // Show which trader to return to
                        string turnInName = def.traderId;
                        if (cfg)
                        {
                            foreach (QT_TraderDef td2 : cfg.TraderNPCPositions)
                            {
                                if (td2.id == def.traderId)
                                {
                                    turnInName = td2.name;
                                    break;
                                }
                            }
                        }
                        QT_RPCManager.SendChatLine(player, "  Turn in to: " + turnInName);
                    }
                    any = true;
                }
                if (!any)
                    QT_RPCManager.SendChatLine(player, "[Quest] No active quests. Find a Quest Trader!");
                break;
            }
        }
    }
}

// ============================================================
//  CLIENT
// ============================================================
modded class MissionGameplay
{
    // Trader positions received from server on connect
    private ref array<string>  m_qt_traderIds;
    private ref array<vector>  m_qt_traderPositions;
    private float              m_qt_maxDist = 3.5;
    private ref QT_QuestMenu  m_questMenu;  // chat-based, not UIScriptedMenu
    private ref QT_QuestHUD   m_questHUD;
    private ref QT_QuestLog   m_questLog;
    private ref QT_AdminPanel m_adminPanel;
    private bool m_qt_initDone = false;
    private Widget            m_toastRoot;
    private string            m_qt_nearbyTraderId = "";
    private bool              m_qt_hintShown = false;
    private TextWidget        m_qt_hintWidget = null;

    override void OnInit()
    {
        super.OnInit();
        QT_ClientRPCDispatcherBase.s_instance = new QT_ClientRPCDispatcher();
        LoadTraderPositionsFromConfig();
    }

    override void OnEvent(EventType eventTypeId, Param params)
    {
        // Suppress /q and q from appearing in chat
        // GlobalChatPlus prepends channel prefix so we check the last word
        if (eventTypeId == ChatMessageEventTypeID)
        {
            ChatMessageEventParams chatParams;
            if (Class.CastTo(chatParams, params) && chatParams)
            {
                string msg = chatParams.param3.Trim();
                string lastWord = msg;
                for (int i = msg.Length() - 1; i >= 0; i--)
                {
                    if (msg.Get(i) == " ")
                    {
                        lastWord = msg.Substring(i + 1, msg.Length() - i - 1);
                        break;
                    }
                }
                if (lastWord == "/q" || lastWord == "q")
                    return; // swallow - do not call super, message won't display
            }
        }
        super.OnEvent(eventTypeId, params);
    }

    private void LoadTraderPositionsFromConfig()
    {
        m_qt_traderIds       = new array<string>();
        m_qt_traderPositions = new array<vector>();

        // Always create the hint widget - positions will arrive via TRADER_POSITIONS RPC
        Widget hintRoot = GetGame().GetWorkspace().CreateWidgets("QuestTrader/gui/layouts/QuestHint.layout");
        if (hintRoot) m_qt_hintWidget = TextWidget.Cast(hintRoot.FindAnyWidget("QTHint"));
        if (m_qt_hintWidget) m_qt_hintWidget.Show(false);

        Print("[QuestTrader] Client: hint widget ready, awaiting trader positions from server.");
    }

    private float m_qt_requestTimer = 0;
    private bool  m_qt_positionsRequested = false;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        QT_Toast.GetInstance().Update(timeslice);
        if (!GetGame().IsClient()) return;

        // After 3 seconds, request trader positions from server (ensures RPC dispatcher is ready)
        if (!m_qt_positionsRequested)
        {
            m_qt_requestTimer += timeslice;
            if (m_qt_requestTimer >= 3.0)
            {
                m_qt_positionsRequested = true;
                QT_RPCManager.RequestTraderPositions();
            }
        }

        if (!m_qt_traderIds || m_qt_traderIds.Count() == 0) return;
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player) return;
        string closestId = "";
        for (int ti = 0; ti < m_qt_traderIds.Count(); ti++)
        {
            if (vector.Distance(player.GetPosition(), m_qt_traderPositions[ti]) <= m_qt_maxDist)
            {
                closestId = m_qt_traderIds[ti];
                break;
            }
        }
        // Show hint when proximity changes
        if (closestId != m_qt_nearbyTraderId)
        {
            if (m_qt_hintWidget)
                m_qt_hintWidget.Show(closestId != "" && !m_questMenu);
        }
        // Keep hint hidden while menu is open
        if (m_qt_hintWidget && m_questMenu) m_qt_hintWidget.Show(false);

        m_qt_nearbyTraderId = closestId;

        // Fire interact on F press when near trader and menu not open
        if (m_qt_nearbyTraderId != "" && !m_questMenu)
        {
            if (GetGame().GetInput().LocalPress("UAAction", false))
                QT_RPCManager.RequestInteractTrader(m_qt_nearbyTraderId);
        }
    }

    void QT_CloseQuestMenu()
    {
        if (m_questMenu)
        {
            GetGame().GetUIManager().HideScriptedMenu(m_questMenu);
            m_questMenu = null;
        }
        if (m_qt_hintWidget && m_qt_nearbyTraderId != "") m_qt_hintWidget.Show(true);
    }

    // Called by QT_DayZGameHook when the client receives an RPC
    void QT_HandleClientRPC(int rpc_type, ParamsReadContext ctx)
    {
        if (rpc_type == QT_RPC.TRADER_POSITIONS) { ParseTraderPositions(ctx); return; }
        if (rpc_type == QT_RPC.PROXIMITY_UPDATE) { ParseProximityUpdate(ctx); return; }
        if (rpc_type == QT_RPC.SEND_QUEST_LIST)    ParseAndShowQuestMenu(ctx);
        else if (rpc_type == QT_RPC.HUD_UPDATE)    ParseAndUpdateHUD(ctx);
        else if (rpc_type == QT_RPC.TOAST)         ParseAndShowToast(ctx);
        else if (rpc_type == QT_RPC.QUEST_LOG)     ParseAndShowQuestLog(ctx);
        else if (rpc_type == QT_RPC.ADMIN_PLAYER_DATA) ParseAndShowAdminPlayers(ctx);
        else if (rpc_type == QT_RPC.ADMIN_LOG_DATA)    ParseAndShowAdminLog(ctx);
    }

    private void ParseAndShowQuestMenu(ParamsReadContext ctx)
    {
        string traderId, greeting, traderName;
        int questCount;
        if (!ctx.Read(traderId))    return;
        if (!ctx.Read(greeting))    return;
        if (!ctx.Read(traderName))  return;
        if (!ctx.Read(questCount))  return;

        ref array<ref QT_QuestEntryUI> quests = new array<ref QT_QuestEntryUI>();

        for (int q = 0; q < questCount; q++)
        {
            ref QT_QuestEntryUI entry = new QT_QuestEntryUI();
            int typeInt, stateInt, objCount, rewCount, cdRemaining;

            ctx.Read(entry.questId);
            ctx.Read(entry.title);
            ctx.Read(entry.description);
            ctx.Read(typeInt);    entry.type  = typeInt;
            ctx.Read(stateInt);   entry.state = stateInt;
            ctx.Read(entry.acceptMessage);
            ctx.Read(cdRemaining); entry.cooldownRemaining = cdRemaining;

            ctx.Read(objCount);
            for (int o = 0; o < objCount; o++)
            {
                string desc; int req, prog;
                ctx.Read(desc); ctx.Read(req); ctx.Read(prog);
                entry.objectiveDescs.Insert(desc);
                entry.objectiveRequired.Insert(req);
                entry.objectiveProgress.Insert(prog);
            }

            ctx.Read(rewCount);
            for (int r = 0; r < rewCount; r++)
            {
                string rewClass; int rewAmt;
                ctx.Read(rewClass); ctx.Read(rewAmt);
                entry.rewardDescs.Insert(rewAmt.ToString() + "x " + rewClass);
            }

            entry.greeting = greeting;
            quests.Insert(entry);
        }

        if (!m_questMenu)
        {
            m_questMenu = new QT_QuestMenu();
            GetGame().GetUIManager().ShowScriptedMenu(m_questMenu, null);
        }
        m_questMenu.SetQuestData(traderId, traderName, quests);
        if (m_qt_hintWidget) m_qt_hintWidget.Show(false);
    }

    private void ParseAndUpdateHUD(ParamsReadContext ctx)
    {
        int questCount;
        if (!ctx.Read(questCount)) return;

        ref array<ref QT_HUDQuestEntry> entries = new array<ref QT_HUDQuestEntry>();

        for (int q = 0; q < questCount; q++)
        {
            ref QT_HUDQuestEntry entry = new QT_HUDQuestEntry();
            int stateInt, typeInt, objCount;

            ctx.Read(entry.title);
            ctx.Read(stateInt); entry.state = stateInt;
            ctx.Read(typeInt);
            ctx.Read(objCount);

            for (int o = 0; o < objCount; o++)
            {
                string desc; int req, prog;
                ctx.Read(desc); ctx.Read(req); ctx.Read(prog);
                string tick = "[ ] ";
                if (prog >= req) tick = "[x] ";
                string line = tick + desc;
                if (typeInt == QT_QuestType.KILL)
                    line = line + " (" + prog.ToString() + "/" + req.ToString() + ")";
                entry.objectiveLines.Insert(line);
            }
            entries.Insert(entry);
        }

        if (m_questHUD) m_questHUD.SetEntries(entries);
    }

    private void ParseAndShowToast(ParamsReadContext ctx)
    {
        string message;
        int toastType;
        if (!ctx.Read(message))   return;
        if (!ctx.Read(toastType)) return;
        QT_Toast.GetInstance().Push(message, toastType);
    }

    private void ParseAndShowQuestLog(ParamsReadContext ctx)
    {
        int histCount;
        if (!ctx.Read(histCount)) return;

        ref array<ref QT_QuestLogEntry> history = new array<ref QT_QuestLogEntry>();
        for (int h = 0; h < histCount; h++)
        {
            ref QT_QuestLogEntry e = new QT_QuestLogEntry();
            ctx.Read(e.questTitle);
            ctx.Read(e.completedAt);
            ctx.Read(e.rewardSummary);
            ctx.Read(e.runNumber);
            history.Insert(e);
        }

        int lbCount;
        ctx.Read(lbCount);
        ref array<string> lb = new array<string>();
        for (int l = 0; l < lbCount; l++)
        {
            string line; ctx.Read(line);
            lb.Insert(line);
        }

        if (m_questLog)
        {
            GetGame().GetUIManager().HideScriptedMenu(m_questLog);
            m_questLog = null;
        }
        m_questLog = new QT_QuestLog();
        GetGame().GetUIManager().ShowScriptedMenu(m_questLog, null);
        m_questLog.SetData(history, lb);
    }

    private void ParseAndShowAdminPlayers(ParamsReadContext ctx)
    {
        int count;
        if (!ctx.Read(count)) return;

        ref array<ref QT_AdminPlayerEntry> players = new array<ref QT_AdminPlayerEntry>();
        for (int i = 0; i < count; i++)
        {
            ref QT_AdminPlayerEntry p = new QT_AdminPlayerEntry();
            ctx.Read(p.uid);
            ctx.Read(p.name);
            ctx.Read(p.activeQuests);
            ctx.Read(p.totalCompleted);
            players.Insert(p);
        }

        if (!m_adminPanel)
        {
            m_adminPanel = new QT_AdminPanel();
            GetGame().GetUIManager().ShowScriptedMenu(m_adminPanel, null);
        }
        m_adminPanel.SetPlayerData(players);
    }


    private void ParseProximityUpdate(ParamsReadContext ctx)
    {
        string traderId;
        if (!ctx.Read(traderId)) return;
        m_qt_nearbyTraderId = traderId;
        if (m_qt_hintWidget)
            m_qt_hintWidget.Show(traderId != "" && !m_questMenu);
    }

    // Called by OnUpdate - check for F key when near a trader
    private void ParseTraderPositions(ParamsReadContext ctx)
    {
        int count;
        if (!ctx.Read(count)) return;
        m_qt_traderIds       = new array<string>();
        m_qt_traderPositions = new array<vector>();
        for (int i = 0; i < count; i++)
        {
            string tid; vector pos; float dist;
            ctx.Read(tid); ctx.Read(pos); ctx.Read(dist);
            m_qt_traderIds.Insert(tid);
            m_qt_traderPositions.Insert(pos);
            m_qt_maxDist = dist;
        }
        Print("[QuestTrader] Client received " + count + " trader positions.");
    }

    private void ParseAndShowAdminLog(ParamsReadContext ctx)
    {
        int count;
        if (!ctx.Read(count)) return;

        ref array<string> lines = new array<string>();
        for (int i = 0; i < count; i++)
        {
            string line; ctx.Read(line);
            lines.Insert(line);
        }

        if (m_adminPanel) m_adminPanel.SetLogLines(lines);
    }
}
