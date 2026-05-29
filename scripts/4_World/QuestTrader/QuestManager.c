// ============================================================
//  QuestTrader | QuestManager.c  (v1.5)
//  QT_ToastType now defined in QuestData.c so both
//  4_World and 5_Mission can use it.
// ============================================================

class QT_OnlinePlayerInfo
{
    string key;
    string uid;
    string name;
    string steamId;
    PlayerBase player;
}

class QT_QuestManager
{
    private static ref QT_QuestManager s_instance;
    private ref QT_Config m_config;
    private ref map<string, ref map<string, ref QT_PlayerQuestState>> m_playerStates;
    private ref QT_ItemSettings m_itemSettings;
    private ref QT_JournalStoriesConfig m_journalStories;
    private ref map<string, ref QT_OnlinePlayerInfo> m_onlinePlayers;
    // uid+questId -> spawned world object (for cleanup on cancel/complete)
    private ref map<string, Object> m_spawnedQuestItems;
    // uid+questId -> spawned world objects (items or interaction props)
    private ref map<string, ref array<Object>> m_spawnedQuestObjects;
    // uid+questId -> array of spawned entities (animals/zombies) for reset on reaccept
    private ref map<string, ref array<Object>> m_spawnedQuestEntities;
    // victim signature + target token -> match result
    private ref map<string, bool> m_killMatchCache;

    // --------------------------------------------------------
    //  Estado de attacker/credit por entidade (substitui as
    //  variáveis de instância que estavam em modded class EntityAI).
    //  Chave: string do ID interno da entidade.
    // --------------------------------------------------------
    // Attacker tracking: EntityLowId -> Player UID (string)
    // Guardamos o UID string e não o ponteiro ao PlayerBase para evitar
    // referência inválida caso o jogador desconecte durante o bleed-out.
    private ref map<int, string> m_qt_lastAttackerUID;
    private ref map<int, int>    m_qt_lastHitTime;
    private ref map<int, int>    m_qt_creditedKillTime;
    private ref map<string, bool>  m_DirtyPlayers;
    private ref map<string, float> m_LastSaveRequestTime;
    private ref array<string>      m_SaveQueue;

    static QT_QuestManager GetInstance()
    {
        if (!s_instance) s_instance = new QT_QuestManager();
        return s_instance;
    }

    void QT_QuestManager()
    {
        // Register RPC dispatcher so 3_Game hook can call into 4_World
        QT_RPCDispatcherBase.s_instance = new QT_RPCDispatcher();
        m_playerStates      = new map<string, ref map<string, ref QT_PlayerQuestState>>();
        m_onlinePlayers     = new map<string, ref QT_OnlinePlayerInfo>();
        m_spawnedQuestItems    = new map<string, Object>();
        m_spawnedQuestObjects  = new map<string, ref array<Object>>();
        m_spawnedQuestEntities = new map<string, ref array<Object>>();
        m_killMatchCache    = new map<string, bool>();
        m_qt_lastAttackerUID   = new map<int, string>();
        m_qt_lastHitTime       = new map<int, int>();
        m_qt_creditedKillTime  = new map<int, int>();
        m_DirtyPlayers         = new map<string, bool>();
        m_LastSaveRequestTime  = new map<string, float>();
        m_SaveQueue            = new array<string>();
        m_config            = QT_ConfigLoader.LoadConfig();
        m_itemSettings      = QT_ItemSettingsLoader.LoadItemSettings();
        m_journalStories    = QT_JournalStoriesLoader.LoadStories();
        int qc = 0;
        if (m_config) qc = m_config.Quests.Count();
        QT_Logger.GetInstance().Info("SYSTEM", "QuestManager initialised. " + qc + " quests loaded.");
    }

    QT_Config GetConfig() { return m_config; }

    bool ReloadConfig()
    {
        SaveAll();

        ref QT_Config newConfig = QT_ConfigLoader.LoadConfig();
        if (!newConfig)
        {
            QT_Logger.GetInstance().Error("ADMIN", "Config reload failed: loader returned null.");
            return false;
        }

        ref QT_ItemSettings newItemSettings = QT_ItemSettingsLoader.LoadItemSettings();
        if (!newItemSettings)
            newItemSettings = QT_ItemSettingsLoader.BuildDefaultItemSettings();

        m_config = newConfig;
        m_itemSettings = newItemSettings;
        m_journalStories = QT_JournalStoriesLoader.LoadStories();
        m_killMatchCache.Clear();
        SyncLoadedPlayerStatesWithConfig();

        QT_Logger.GetInstance().Info("ADMIN", "Config reloaded. " + m_config.TraderNPCPositions.Count() + " traders, " + m_config.Quests.Count() + " quests.");
        return true;
    }

    string GetJournalStoryOverride(string questId)
    {
        if (questId == "") return "";
        if (!m_journalStories || !m_journalStories.stories) return "";
        if (!m_journalStories.stories.Contains(questId)) return "";
        return m_journalStories.stories.Get(questId);
    }

    array<string> GetJournalOrder()
    {
        if (!m_journalStories || !m_journalStories.order) return new array<string>();
        return m_journalStories.order;
    }

    private void SyncLoadedPlayerStatesWithConfig()
    {
        foreach (string uid, map<string, ref QT_PlayerQuestState> questMap : m_playerStates)
        {
            foreach (string questId, QT_PlayerQuestState qs : questMap)
            {
                QT_QuestDef def = GetQuestDef(questId);
                if (!def || !qs || !qs.objectiveProgress) continue;

                while (qs.objectiveProgress.Count() < def.objectives.Count())
                    qs.objectiveProgress.Insert(0);

                while (qs.objectiveProgress.Count() > def.objectives.Count())
                    qs.objectiveProgress.Remove(qs.objectiveProgress.Count() - 1);
            }
        }
    }

    void OnPlayerConnected(string uid)
    {
        if (m_playerStates.Contains(uid))
            return;

        ref map<string, ref QT_PlayerQuestState> loaded = QT_PersistenceManager.LoadPlayer(uid);
        if (loaded)
            m_playerStates.Insert(uid, loaded);
        else
            m_playerStates.Insert(uid, new map<string, ref QT_PlayerQuestState>());
        QT_Logger.GetInstance().Info("CONNECT", "Player connected.", uid);
    }

    void RegisterOnlinePlayer(PlayerBase player, PlayerIdentity identity = null, bool notifyAdmins = true)
    {
        if (!GetGame().IsServer()) return;
        if (!identity && player)
            identity = player.GetIdentity();
        if (!identity) return;

        string uid = identity.GetId();
        string steamId = identity.GetPlainId();
        string key = BuildOnlinePlayerKey(uid, steamId);
        if (key == "") return;
        if (uid == "") uid = key;
        OnPlayerConnected(uid);

        bool isNewOnline = !m_onlinePlayers.Contains(key);
        ref QT_OnlinePlayerInfo info;
        if (m_onlinePlayers.Contains(key))
            info = m_onlinePlayers.Get(key);
        else
        {
            info = new QT_OnlinePlayerInfo();
            m_onlinePlayers.Insert(key, info);
        }

        info.key = key;
        info.uid = uid;
        info.name = identity.GetName();
        info.steamId = steamId;
        if (player)
            info.player = player;

        if (isNewOnline && notifyAdmins)
        {
            QT_Logger.GetInstance().Info("CONNECT", "[QuestTraderAdmin] Added player: " + info.name, uid);
        }
    }

    void RefreshOnlinePlayersFromServer()
    {
        if (!GetGame().IsServer()) return;

        ref map<string, bool> seenKeys = new map<string, bool>();
        ref array<PlayerIdentity> identities = new array<PlayerIdentity>();
        GetGame().GetPlayerIndentities(identities);
        bool hasAuthoritativeIdentities = identities.Count() > 0;
        foreach (PlayerIdentity identity : identities)
        {
            if (!identity) continue;
            RegisterOnlinePlayer(null, identity, false);
            string identityKey = BuildOnlinePlayerKey(identity.GetId(), identity.GetPlainId());
            if (identityKey != "") seenKeys.Set(identityKey, true);
        }

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);
        foreach (Man gameMan : players)
        {
            PlayerBase gamePlayer = PlayerBase.Cast(gameMan);
            if (!gamePlayer || !gamePlayer.GetIdentity()) continue;
            RegisterOnlinePlayer(gamePlayer, gamePlayer.GetIdentity(), false);
            string gameKey = BuildOnlinePlayerKey(gamePlayer.GetIdentity().GetId(), gamePlayer.GetIdentity().GetPlainId());
            if (gameKey != "") seenKeys.Set(gameKey, true);
        }

        players.Clear();
        GetGame().GetWorld().GetPlayerList(players);
        foreach (Man worldMan : players)
        {
            PlayerBase worldPlayer = PlayerBase.Cast(worldMan);
            if (!worldPlayer || !worldPlayer.GetIdentity()) continue;
            RegisterOnlinePlayer(worldPlayer, worldPlayer.GetIdentity(), false);
            string worldKey = BuildOnlinePlayerKey(worldPlayer.GetIdentity().GetId(), worldPlayer.GetIdentity().GetPlainId());
            if (worldKey != "") seenKeys.Set(worldKey, true);
        }

        if (hasAuthoritativeIdentities && seenKeys.Count() > 0)
        {
            ref array<string> removeKeys = new array<string>();
            foreach (string onlineKey, QT_OnlinePlayerInfo info : m_onlinePlayers)
            {
                if (!seenKeys.Contains(onlineKey))
                    removeKeys.Insert(onlineKey);
            }

            foreach (string removeKey : removeKeys)
            {
                QT_OnlinePlayerInfo removed = m_onlinePlayers.Get(removeKey);
                string removedName = removeKey;
                if (removed && removed.name != "") removedName = removed.name;
                m_onlinePlayers.Remove(removeKey);
                QT_Logger.GetInstance().Info("CONNECT", "[QuestTraderAdmin] Removed player: " + removedName);
            }
        }
    }

    void OnPlayerDisconnected(string uid)
    {
        bool wasKnown = false;
        if (m_playerStates.Contains(uid))
        {
            wasKnown = true;
            auto questMap = m_playerStates.Get(uid);
            if (questMap)
            {
                if (SavePlayerImmediate(uid, "disconnect"))
                    m_playerStates.Remove(uid);
                else
                    MarkPlayerDirty(uid);
            }
            else
            {
                m_playerStates.Remove(uid);
            }
        }
        ref array<string> removeKeys = new array<string>();
        foreach (string onlineKey, QT_OnlinePlayerInfo info : m_onlinePlayers)
        {
            if (!info) continue;
            if (onlineKey == uid || info.uid == uid || info.steamId == uid)
                removeKeys.Insert(onlineKey);
        }

        foreach (string removeKey : removeKeys)
        {
            wasKnown = true;
            m_onlinePlayers.Remove(removeKey);
        }
        if (wasKnown)
        {
            QT_Logger.GetInstance().Info("CONNECT", "Player disconnected.", uid);
        }
    }

    void GetOnlinePlayers(array<PlayerBase> output)
    {
        if (!output) return;

        foreach (string uid, QT_OnlinePlayerInfo info : m_onlinePlayers)
        {
            if (info && info.player)
                output.Insert(info.player);
        }
    }

    void GetOnlinePlayerInfos(array<ref QT_OnlinePlayerInfo> output)
    {
        if (!output) return;

        foreach (string uid, QT_OnlinePlayerInfo info : m_onlinePlayers)
        {
            if (info && info.uid != "")
                output.Insert(info);
        }
    }

    private string BuildOnlinePlayerKey(string uid, string steamId)
    {
        steamId = steamId.Trim();
        if (steamId != "") return steamId;

        uid = uid.Trim();
        return uid;
    }

    void SaveAll()
    {
        QT_Perf.Log("Full backup started players=" + m_playerStates.Count().ToString());
        int saved = 0;
        int failed = 0;
        foreach (string uid, map<string, ref QT_PlayerQuestState> questMap : m_playerStates)
        {
            if (QT_PersistenceManager.ForceSavePlayer(uid, questMap))
            {
                saved++;
                if (m_DirtyPlayers.Contains(uid)) m_DirtyPlayers.Set(uid, false);
                RemoveFromSaveQueue(uid);
            }
            else
            {
                failed++;
                MarkPlayerDirty(uid);
            }
        }
        QT_Perf.Log("Full backup finished saved=" + saved.ToString() + " failed=" + failed.ToString());
        QT_Logger.GetInstance().Info("PERSIST", "Full player backup saved.");
    }

    void ProcessSaveQueue()
    {
        if (!GetGame().IsServer()) return;
        if (!m_SaveQueue || m_SaveQueue.Count() == 0)
        {
            QT_Perf.Log("Save queue size=0");
            return;
        }

        int maxPerTick = QT_Perf.SaveMaxPlayersPerTick();
        int processed = 0;
        float now = GetGame().GetTime() / 1000.0;
        QT_Perf.Log("Save queue size=" + m_SaveQueue.Count().ToString());

        for (int i = m_SaveQueue.Count() - 1; i >= 0 && processed < maxPerTick; i--)
        {
            string uid = m_SaveQueue[i];
            if (uid == "" || !m_DirtyPlayers.Contains(uid) || !m_DirtyPlayers.Get(uid))
            {
                m_SaveQueue.Remove(i);
                continue;
            }

            float lastRequest = 0;
            if (m_LastSaveRequestTime.Contains(uid))
                lastRequest = m_LastSaveRequestTime.Get(uid);

            float debounce = QT_Perf.SaveDebounceSeconds();
            if (now - lastRequest < debounce)
            {
                QT_Perf.Log("Player save skipped by debounce uid=" + uid);
                continue;
            }

            processed++;
            if (SavePlayerImmediate(uid, "queue"))
            {
                RemoveFromSaveQueue(uid);
            }
        }
    }

    void MarkPlayerDirty(string uid)
    {
        if (uid == "") return;
        if (!m_DirtyPlayers) m_DirtyPlayers = new map<string, bool>();
        if (!m_LastSaveRequestTime) m_LastSaveRequestTime = new map<string, float>();
        if (!m_SaveQueue) m_SaveQueue = new array<string>();

        m_DirtyPlayers.Set(uid, true);
        m_LastSaveRequestTime.Set(uid, GetGame().GetTime() / 1000.0);
        QT_Perf.Log("Player marked dirty uid=" + uid);

        if (!IsPlayerQueuedForSave(uid))
        {
            m_SaveQueue.Insert(uid);
            QT_Perf.Log("Player queued for save uid=" + uid);
        }
    }

    bool SavePlayerImmediate(string uid, string reason = "manual")
    {
        if (uid == "") return false;
        if (!m_playerStates.Contains(uid)) return false;

        map<string, ref QT_PlayerQuestState> questMap = m_playerStates.Get(uid);
        if (!questMap) return false;

        bool ok = QT_PersistenceManager.ForceSavePlayer(uid, questMap);
        if (!ok) return false;

        if (m_DirtyPlayers.Contains(uid)) m_DirtyPlayers.Set(uid, false);
        RemoveFromSaveQueue(uid);
        if (reason == "queue" && !IsPlayerOnline(uid))
            m_playerStates.Remove(uid);
        QT_Perf.Log("Player saved successfully uid=" + uid + " reason=" + reason);
        return true;
    }

    private bool IsPlayerOnline(string uid)
    {
        foreach (string onlineKey, QT_OnlinePlayerInfo info : m_onlinePlayers)
        {
            if (!info) continue;
            if (onlineKey == uid || info.uid == uid || info.steamId == uid)
                return true;
        }
        return false;
    }

    private bool IsPlayerQueuedForSave(string uid)
    {
        if (!m_SaveQueue) return false;
        foreach (string queuedUid : m_SaveQueue)
        {
            if (queuedUid == uid) return true;
        }
        return false;
    }

    private void RemoveFromSaveQueue(string uid)
    {
        if (!m_SaveQueue) return;
        for (int i = m_SaveQueue.Count() - 1; i >= 0; i--)
        {
            if (m_SaveQueue[i] == uid)
                m_SaveQueue.Remove(i);
        }
    }

    ref map<string, ref QT_PlayerQuestState> GetOrCreatePlayerMap(string uid)
    {
        if (!m_playerStates.Contains(uid))
        {
            // Try loading from disk first before creating a blank map
            ref map<string, ref QT_PlayerQuestState> loaded = QT_PersistenceManager.LoadPlayer(uid);
            if (loaded)
            {
                m_playerStates.Insert(uid, loaded);
            }
            else
            {
                m_playerStates.Insert(uid, new map<string, ref QT_PlayerQuestState>());
            }
        }
        return m_playerStates.Get(uid);
    }

    ref QT_PlayerQuestState GetPlayerQuestState(string uid, string questId)
    {
        auto playerMap = GetOrCreatePlayerMap(uid);
        if (!playerMap.Contains(questId))
        {
            ref QT_PlayerQuestState qs = new QT_PlayerQuestState();
            qs.questId = questId;
            QT_QuestDef def = GetQuestDef(questId);
            if (def)
            {
                foreach (QT_Objective obj : def.objectives)
                    qs.objectiveProgress.Insert(0);
            }
            playerMap.Insert(questId, qs);
        }
        return playerMap.Get(questId);
    }

    ref QT_PlayerQuestState GetExistingPlayerQuestState(string uid, string questId)
    {
        auto playerMap = GetOrCreatePlayerMap(uid);
        if (!playerMap || !playerMap.Contains(questId)) return null;
        return playerMap.Get(questId);
    }

    private void EnsureObjectiveProgress(QT_PlayerQuestState qs, QT_QuestDef def)
    {
        if (!qs || !def || !qs.objectiveProgress) return;

        while (qs.objectiveProgress.Count() < def.objectives.Count())
            qs.objectiveProgress.Insert(0);

        while (qs.objectiveProgress.Count() > def.objectives.Count())
            qs.objectiveProgress.Remove(qs.objectiveProgress.Count() - 1);
    }

    QT_QuestDef GetQuestDef(string questId)
    {
        if (!m_config) return null;
        foreach (QT_QuestDef def : m_config.Quests)
            if (def.id == questId) return def;
        return null;
    }

    QT_QuestDef ResolveQuestForAdmin(string uid, string input)
    {
        if (!m_config) return null;

        string trimmed = input.Trim();
        if (trimmed == "")
        {
            array<ref QT_QuestDef> active = GetActiveQuestsForPlayer(uid);
            if (active.Count() > 0) return active[0];
            return null;
        }

        foreach (QT_QuestDef defById : m_config.Quests)
            if (defById.id == trimmed) return defById;

        string inputLower = trimmed;
        inputLower.ToLower();
        foreach (QT_QuestDef defByTitle : m_config.Quests)
        {
            string titleLower = defByTitle.title;
            titleLower.ToLower();
            if (titleLower == inputLower) return defByTitle;
        }

        foreach (QT_QuestDef defPartial : m_config.Quests)
        {
            string partialTitle = defPartial.title;
            partialTitle.ToLower();
            if (partialTitle.Contains(inputLower)) return defPartial;
        }

        return null;
    }

    array<ref QT_QuestDef> GetQuestsForTrader(string traderId, string playerUID)
    {
        array<ref QT_QuestDef> result = new array<ref QT_QuestDef>();
        if (!m_config) return result;
        foreach (QT_QuestDef def : m_config.Quests)
        {
            QT_QuestState state = GetEffectiveStateNoCreate(playerUID, def.id);

            // Hide locked quests. A quest is visible only when it can be
            // accepted now, is already active/ready, or belongs in DONE.
            if (!ArePrerequisitesMet(playerUID, def))
            {
                if (state != QT_QuestState.ACTIVE && state != QT_QuestState.COMPLETED && state != QT_QuestState.TURNED_IN && state != QT_QuestState.COOLDOWN)
                    continue;
            }

            // Normal quests: show at their origin trader
            if (def.traderId == traderId)
            {
                result.Insert(def);
                continue;
            }
            // Delivery quests: also show at the delivery trader when active or completed
            if (def.type == QT_QuestType.DELIVER && def.deliveryTraderId == traderId)
            {
                if (state == QT_QuestState.ACTIVE || state == QT_QuestState.COMPLETED)
                    result.Insert(def);
            }
        }
        return result;
    }

    bool ArePrerequisitesMet(string uid, QT_QuestDef def)
    {
        if (!def) return false;
        foreach (string preReqId : def.prerequisiteQuestIds)
        {
            if (GetEffectiveStateNoCreate(uid, preReqId) != QT_QuestState.TURNED_IN)
                return false;
        }
        return true;
    }

    array<ref QT_QuestDef> GetActiveQuestsForPlayer(string uid)
    {
        array<ref QT_QuestDef> result = new array<ref QT_QuestDef>();
        if (!m_config) return result;
        auto playerMap = GetOrCreatePlayerMap(uid);
        foreach (string questId, QT_PlayerQuestState qs : playerMap)
        {
            if (!qs) continue;
            QT_QuestState st = GetEffectiveStateNoCreate(uid, questId);
            if (st == QT_QuestState.ACTIVE || st == QT_QuestState.COMPLETED)
            {
                QT_QuestDef def = GetQuestDef(questId);
                if (def) result.Insert(def);
            }
        }
        return result;
    }

    QT_QuestState GetDisplayState(PlayerBase player, QT_QuestDef def)
    {
        if (!player || !player.GetIdentity() || !def)
            return QT_QuestState.AVAILABLE;

        QT_QuestState state = GetEffectiveStateNoCreate(player.GetIdentity().GetId(), def.id);
        if (state != QT_QuestState.ACTIVE && state != QT_QuestState.COMPLETED)
            return state;

        if (def.type != QT_QuestType.COLLECT && def.type != QT_QuestType.DELIVER)
            return state;

        if (AreInventoryObjectivesReady(player, def))
            return QT_QuestState.COMPLETED;

        return QT_QuestState.ACTIVE;
    }

    bool AreInventoryObjectivesReady(PlayerBase player, QT_QuestDef def)
    {
        if (!player || !def) return false;

        if (def.type == QT_QuestType.DELIVER && def.objectives.Count() == 0)
            return HasDeliveryItem(player, def);

        if (def.objectives.Count() == 0) return false;

        foreach (QT_Objective obj : def.objectives)
        {
            if (!obj || obj.itemClassName == "") return false;
            if (CountItemsInInventory(player, obj.itemClassName) < obj.requiredAmount)
                return false;
        }
        return true;
    }

    string BuildInventoryQuestSignature(PlayerBase player)
    {
        if (!player || !player.GetIdentity()) return "";

        string uid = player.GetIdentity().GetId();
        string sig = "";
        array<ref QT_QuestDef> active = GetActiveQuestsForPlayer(uid);
        foreach (QT_QuestDef def : active)
        {
            if (!def) continue;
            if (def.type != QT_QuestType.COLLECT && def.type != QT_QuestType.DELIVER) continue;

            sig = sig + def.id + ":" + ((int)GetDisplayState(player, def)).ToString() + ":";

            if (def.type == QT_QuestType.DELIVER && def.objectives.Count() == 0)
            {
                int deliveryCount = CountItemsInInventory(player, def.deliveryItemClass);
                if (deliveryCount > 1) deliveryCount = 1;
                sig = sig + deliveryCount.ToString() + "/1;";
                continue;
            }

            foreach (QT_Objective obj : def.objectives)
            {
                int count = CountItemsInInventory(player, obj.itemClassName);
                if (count > obj.requiredAmount) count = obj.requiredAmount;
                sig = sig + count.ToString() + "/" + obj.requiredAmount.ToString() + ",";
            }
            sig = sig + ";";
        }
        return sig;
    }

    bool HasActiveInventoryQuest(string uid)
    {
        auto playerMap = GetOrCreatePlayerMap(uid);
        foreach (string questId, QT_PlayerQuestState qs : playerMap)
        {
            if (!qs) continue;
            if (qs.state != QT_QuestState.ACTIVE && qs.state != QT_QuestState.COMPLETED) continue;

            QT_QuestDef def = GetQuestDef(questId);
            if (!def) continue;
            if (def.type == QT_QuestType.COLLECT || def.type == QT_QuestType.DELIVER)
                return true;
        }
        return false;
    }

    string BuildReadyInventoryQuestSignature(PlayerBase player)
    {
        if (!player || !player.GetIdentity()) return "";

        string uid = player.GetIdentity().GetId();
        string sig = "";
        array<ref QT_QuestDef> active = GetActiveQuestsForPlayer(uid);
        foreach (QT_QuestDef def : active)
        {
            if (!def) continue;
            if (def.type != QT_QuestType.COLLECT && def.type != QT_QuestType.DELIVER) continue;
            if (GetDisplayState(player, def) == QT_QuestState.COMPLETED)
                sig = sig + def.id + ";";
        }
        return sig;
    }

    QT_QuestState GetEffectiveState(string uid, string questId)
    {
        auto qs = GetPlayerQuestState(uid, questId);
        return ResolveEffectiveState(qs, uid, questId);
    }

    QT_QuestState GetEffectiveStateNoCreate(string uid, string questId)
    {
        auto qs = GetExistingPlayerQuestState(uid, questId);
        if (!qs) return QT_QuestState.AVAILABLE;
        return ResolveEffectiveState(qs, uid, questId);
    }

    private QT_QuestState ResolveEffectiveState(QT_PlayerQuestState qs, string uid, string questId)
    {
        if (!qs) return QT_QuestState.AVAILABLE;
        QT_QuestDef stateDef = GetQuestDef(questId);
        if (stateDef) EnsureObjectiveProgress(qs, stateDef);

        if (qs.state == QT_QuestState.COOLDOWN)
        {
            QT_QuestDef def = stateDef;
            if (def && def.repeatable)
            {
                int elapsed = GetGame().GetTime() / 1000 - qs.completedTimestamp;
                if (elapsed >= def.cooldownHours * 3600)
                {
                    qs.state = QT_QuestState.AVAILABLE;
                    foreach (int i, int v : qs.objectiveProgress)
                        qs.objectiveProgress.Set(i, 0);
                }
            }
        }
        // Auto-promote kill quests to COMPLETED if all objectives met
        if (qs.state == QT_QuestState.ACTIVE)
        {
            QT_QuestDef kDef = stateDef;
            if (kDef && kDef.type == QT_QuestType.KILL)
            {
                bool allKilled = true;
                for (int ki = 0; ki < kDef.objectives.Count(); ki++)
                {
                    int kReq = kDef.objectives[ki].requiredAmount;
                    int kProg = 0;
                    if (ki < qs.objectiveProgress.Count()) kProg = qs.objectiveProgress[ki];
                    if (kProg < kReq) { allKilled = false; break; }
                }
                if (allKilled) qs.state = QT_QuestState.COMPLETED;
            }
        }

        return qs.state;
    }

    bool AcceptQuest(PlayerBase player, string questId)
    {
        if (!player) return false;
        string uid = player.GetIdentity().GetId();

        QT_QuestDef def = GetQuestDef(questId);
        if (!def) return false;

        // Allow one active/ready quest per origin trader.
        auto playerMap = GetOrCreatePlayerMap(uid);
        foreach (string existingId, QT_PlayerQuestState existingQs : playerMap)
        {
            if (existingId == questId) continue; // don't check the quest being accepted
            QT_QuestState effectiveState = GetEffectiveState(uid, existingId);
            if (effectiveState != QT_QuestState.ACTIVE && effectiveState != QT_QuestState.COMPLETED) continue;

            QT_QuestDef existingDef = GetQuestDef(existingId);
            if (!existingDef || existingDef.traderId != def.traderId) continue;

            QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_FINISH_CURRENT_TRADER", QT_ToastType.WARNING);
            return false;
        }

        foreach (string preReqId : def.prerequisiteQuestIds)
        {
            if (GetEffectiveStateNoCreate(uid, preReqId) != QT_QuestState.TURNED_IN)
            {
                QT_QuestDef pre = GetQuestDef(preReqId);
                string preName = preReqId;
                if (pre) preName = pre.title;
                QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_COMPLETE_PREREQ " + preName, QT_ToastType.WARNING);
                QT_RPCManager.SendChatLine(player, "#QuestTrader_CHAT_PREFIX #QuestTrader_CHAT_COMPLETE_BEFORE_ACCEPT: " + preName);
                return false;
            }
        }

        if (GetEffectiveState(uid, questId) != QT_QuestState.AVAILABLE)
        {
            QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_QUEST_NOT_AVAILABLE", QT_ToastType.WARNING);
            return false;
        }

        auto qs = GetPlayerQuestState(uid, questId);
        qs.state = QT_QuestState.ACTIVE;

        // Give delivery item immediately on accept
        if (def.type == QT_QuestType.DELIVER)
            GiveDeliveryItem(player, def);

        MarkPlayerDirty(uid);
        QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_QUEST_ACCEPTED " + def.title, QT_ToastType.ACCEPT);
        QT_RPCManager.SendQuestInfo(player, def);
        QT_RPCManager.SendHUDUpdate(player);
        QT_Logger.GetInstance().Info("QUEST", "Accepted: " + def.title, uid, player.GetIdentity().GetName());

        // Spawn quest entities only when explicitly configured for kill quests
        // and the global kill target spawning setting is enabled
        QT_Config cfg = GetConfig();
        bool spawnEnabled = !cfg || !cfg.Settings || cfg.Settings.EnableKillTargetSpawning;
        if (def.type == QT_QuestType.KILL && def.spawnKillTarget && spawnEnabled)
            SpawnQuestEntities(player, def);

        if (def.type == QT_QuestType.INTERACT && def.spawnInteractionObject)
            SpawnInteractionObject(uid, def);

        // Spawn a world item at a fixed position if defined
        if (def.spawnItemClass != "")
            SpawnQuestItem(uid, def, player);

        // Spawn additional items array if defined
        if (def.spawnItems && def.spawnItems.Count() > 0)
        {
            foreach (QT_SpawnItem si : def.spawnItems)
            {
                vector pos = si.position;
                if (pos[1] == 0) pos[1] = GetGame().SurfaceY(pos[0], pos[2]) + 0.3;
                Object obj = GetGame().CreateObject(si.itemClass, pos, false, true);
                if (obj)
                {
                    TrackQuestObject(uid, def.id, obj);
                    Print("[QuestTrader] Spawned quest item: " + si.itemClass + " at " + pos.ToString());
                }
            }
        }

        return true;
    }

    private void SpawnQuestItem(string uid, QT_QuestDef def, PlayerBase player = null)
    {
        vector pos = def.spawnPosition;

        // Zero position = spawn on or near the player
        if (pos == vector.Zero && player)
        {
            // Try to create directly in inventory first
            EntityAI item = null;
            if (player.GetInventory())
                item = EntityAI.Cast(player.GetInventory().CreateInInventory(def.spawnItemClass));

            if (item)
            {
                string key = uid + "_" + def.id;
                m_spawnedQuestItems.Insert(key, item);
                Print("[QuestTrader] Spawned quest item " + def.spawnItemClass + " in player inventory: " + uid);
                return;
            }

            // Inventory full — drop at player's feet
            pos = player.GetPosition();
            pos[1] = GetGame().SurfaceY(pos[0], pos[2]) + 0.3;
        }
        else if (pos[1] == 0)
        {
            // Position set but Y is 0 — calculate ground height
            pos[1] = GetGame().SurfaceY(pos[0], pos[2]) + 0.5;
        }

        Object obj = GetGame().CreateObject(def.spawnItemClass, pos, false, true);
        if (!obj)
        {
            Print("[QuestTrader] ERROR - Could not spawn quest item: " + def.spawnItemClass + " for quest: " + def.id);
            return;
        }

        string key2 = uid + "_" + def.id;
        m_spawnedQuestItems.Insert(key2, obj);
        Print("[QuestTrader] Spawned quest item: " + def.spawnItemClass + " at " + pos.ToString() + " for player: " + uid);
    }

    private void SpawnInteractionObject(string uid, QT_QuestDef def)
    {
        if (!def || def.interactionObjectClassName == "") return;

        vector pos = def.interactionPosition;
        if (pos == vector.Zero)
        {
            Print("[QuestTrader] Interaction quest requested object spawn without interactionPosition: " + def.id);
            return;
        }
        if (pos[1] == 0)
            pos[1] = GetGame().SurfaceY(pos[0], pos[2]) + 0.1;

        Object obj = GetGame().CreateObject(def.interactionObjectClassName, pos, false, true);
        if (!obj)
        {
            Print("[QuestTrader] ERROR - Could not spawn interaction object: " + def.interactionObjectClassName + " for quest: " + def.id);
            return;
        }

        TrackQuestObject(uid, def.id, obj);
        Print("[QuestTrader] Spawned interaction object: " + def.interactionObjectClassName + " at " + pos.ToString() + " for quest: " + def.id);
    }

    private void TrackQuestObject(string uid, string questId, Object obj)
    {
        if (!obj) return;
        string key = uid + "_" + questId;
        ref array<Object> objects;
        if (m_spawnedQuestObjects.Contains(key))
            objects = m_spawnedQuestObjects.Get(key);
        else
        {
            objects = new array<Object>();
            m_spawnedQuestObjects.Insert(key, objects);
        }
        objects.Insert(obj);
    }

    private void CleanupQuestItem(string uid, string questId)
    {
        string key = uid + "_" + questId;
        if (m_spawnedQuestItems.Contains(key))
        {
            Object obj = m_spawnedQuestItems.Get(key);
            if (obj) GetGame().ObjectDelete(obj);
            m_spawnedQuestItems.Remove(key);
            Print("[QuestTrader] Cleaned up quest item for player: " + uid + " quest: " + questId);
        }

        if (m_spawnedQuestObjects.Contains(key))
        {
            array<Object> objects = m_spawnedQuestObjects.Get(key);
            foreach (Object spawnedObj : objects)
            {
                if (spawnedObj) GetGame().ObjectDelete(spawnedObj);
            }
            m_spawnedQuestObjects.Remove(key);
            Print("[QuestTrader] Cleaned up spawned quest objects for player: " + uid + " quest: " + questId);
        }

        // Also delete and clear tracked entity references on cleanup (turn-in, cancel, complete)
        if (m_spawnedQuestEntities.Contains(key))
        {
            array<Object> entities = m_spawnedQuestEntities.Get(key);
            foreach (Object ent : entities)
            {
                if (ent)
                {
                    EntityAI entAI = EntityAI.Cast(ent);
                    if (entAI && entAI.IsAlive())
                        GetGame().ObjectDelete(ent);
                }
            }
            m_spawnedQuestEntities.Remove(key);
            Print("[QuestTrader] Cleaned up spawned entities for player: " + uid + " quest: " + questId);
        }
    }

    private string ResolveSpawnClass(string entityClassName)
    {
        // Dummy objectives used for recon quests — never spawn these
        if (entityClassName == "QT_ReconObjective") return "";

        // Translate quest entityClassNames to actual spawnable prefixed classnames
        if (entityClassName == "Deer" || entityClassName == "CervusElaphus")
            return "Animal_CervusElaphus";
        if (entityClassName == "CapreolusCapreolus")
            return "Animal_CapreolusCapreolus";
        if (entityClassName == "CanisLupus")
            return "Animal_CanisLupus_Grey";
        if (entityClassName == "SusScrofa")
            return "Animal_SusScrofa";
        if (entityClassName == "SusDomesticus")
            return "Animal_SusDomesticus";
        if (entityClassName == "UrsusArctos")
            return "Animal_UrsusArctos";
        if (entityClassName == "RangiferTarandus")
            return "Animal_RangiferTarandus";
        if (entityClassName == "VulpesVulpes")
            return "Animal_VulpesVulpes";
        if (entityClassName == "LepusEuropaeus")
            return "Animal_LepusEuropaeus";
        if (entityClassName == "OvisAries")
            return "Animal_OvisAries";
        if (entityClassName == "CapraHircus")
            return "Animal_CapraHircus_Brown";
        if (entityClassName == "GallusGallusDomesticus")
            return "Animal_GallusGallusDomesticus";
        if (entityClassName == "BosTaurus")
            return "Animal_BosTaurus_Brown";

        // Infected — "Zmb" and similar partial names are kill-tracking aliases,
        // not real classnames. Map to a random mix of spawnable infected.
        bool isInfectedAlias = (entityClassName == "Zmb" || entityClassName == "ZombieBase" || entityClassName == "Infected" || entityClassName == "infected");
        if (isInfectedAlias)
        {
            // Static cache - built once, reused for every spawn
            if (!s_zmbClassCache || s_zmbClassCache.Count() == 0)
                BuildZmbClassCache();
            return s_zmbClassCache[Math.RandomInt(0, s_zmbClassCache.Count())];
        }

        // Legacy path kept for direct classname pass-through
        if (false) // dead code - replaced by cached path above
        {
            ref array<string> zmbClasses = new array<string>();
            zmbClasses.Insert("ZmbF_BlueCollarFat_Blue");
            zmbClasses.Insert("ZmbF_BlueCollarFat_Green");
            zmbClasses.Insert("ZmbF_BlueCollarFat_Red");
            zmbClasses.Insert("ZmbF_BlueCollarFat_White");
            zmbClasses.Insert("ZmbF_CitizenANormal_Beige");
            zmbClasses.Insert("ZmbF_CitizenANormal_Blue");
            zmbClasses.Insert("ZmbF_CitizenANormal_Brown");
            zmbClasses.Insert("ZmbF_CitizenBSkinny");
            zmbClasses.Insert("ZmbF_ClerkFat_Black");
            zmbClasses.Insert("ZmbF_ClerkFat_BluePattern");
            zmbClasses.Insert("ZmbF_ClerkFat_GreyPattern");
            zmbClasses.Insert("ZmbF_ClerkFat_White");
            zmbClasses.Insert("ZmbF_Clerk_Normal_Blue");
            zmbClasses.Insert("ZmbF_Clerk_Normal_Green");
            zmbClasses.Insert("ZmbF_Clerk_Normal_Red");
            zmbClasses.Insert("ZmbF_Clerk_Normal_White");
            zmbClasses.Insert("ZmbF_DoctorSkinny");
            zmbClasses.Insert("ZmbF_HikerSkinny_Blue");
            zmbClasses.Insert("ZmbF_HikerSkinny_Green");
            zmbClasses.Insert("ZmbF_HikerSkinny_Grey");
            zmbClasses.Insert("ZmbF_HikerSkinny_Red");
            zmbClasses.Insert("ZmbF_JoggerSkinny_Blue");
            zmbClasses.Insert("ZmbF_JoggerSkinny_Brown");
            zmbClasses.Insert("ZmbF_JoggerSkinny_Green");
            zmbClasses.Insert("ZmbF_JoggerSkinny_Red");
            zmbClasses.Insert("ZmbF_JournalistNormal_Blue");
            zmbClasses.Insert("ZmbF_JournalistNormal_Green");
            zmbClasses.Insert("ZmbF_JournalistNormal_Red");
            zmbClasses.Insert("ZmbF_JournalistNormal_White");
            zmbClasses.Insert("ZmbF_MechanicNormal_Beige");
            zmbClasses.Insert("ZmbF_MechanicNormal_Green");
            zmbClasses.Insert("ZmbF_MechanicNormal_Grey");
            zmbClasses.Insert("ZmbF_MechanicNormal_Orange");
            zmbClasses.Insert("ZmbF_MilkMaidOld_Beige");
            zmbClasses.Insert("ZmbF_MilkMaidOld_Black");
            zmbClasses.Insert("ZmbF_MilkMaidOld_Green");
            zmbClasses.Insert("ZmbF_MilkMaidOld_Grey");
            zmbClasses.Insert("ZmbF_NurseFat");
            zmbClasses.Insert("ZmbF_ParamedicNormal_Blue");
            zmbClasses.Insert("ZmbF_ParamedicNormal_Green");
            zmbClasses.Insert("ZmbF_ParamedicNormal_Red");
            zmbClasses.Insert("ZmbF_PatientOld");
            zmbClasses.Insert("ZmbF_PoliceWomanNormal");
            zmbClasses.Insert("ZmbF_ShortSkirt_beige");
            zmbClasses.Insert("ZmbF_ShortSkirt_black");
            zmbClasses.Insert("ZmbF_ShortSkirt_brown");
            zmbClasses.Insert("ZmbF_ShortSkirt_checks");
            zmbClasses.Insert("ZmbF_ShortSkirt_green");
            zmbClasses.Insert("ZmbF_ShortSkirt_grey");
            zmbClasses.Insert("ZmbF_ShortSkirt_red");
            zmbClasses.Insert("ZmbF_ShortSkirt_stripes");
            zmbClasses.Insert("ZmbF_ShortSkirt_white");
            zmbClasses.Insert("ZmbF_ShortSkirt_yellow");
            zmbClasses.Insert("ZmbF_SkaterYoung_Brown");
            zmbClasses.Insert("ZmbF_SkaterYoung_Striped");
            zmbClasses.Insert("ZmbF_SkaterYoung_Violet");
            zmbClasses.Insert("ZmbF_SurvivorNormal_Blue");
            zmbClasses.Insert("ZmbF_SurvivorNormal_Orange");
            zmbClasses.Insert("ZmbF_SurvivorNormal_Red");
            zmbClasses.Insert("ZmbF_SurvivorNormal_White");
            zmbClasses.Insert("ZmbF_VillagerOld_Blue");
            zmbClasses.Insert("ZmbF_VillagerOld_Green");
            zmbClasses.Insert("ZmbF_VillagerOld_Red");
            zmbClasses.Insert("ZmbF_VillagerOld_White");
            zmbClasses.Insert("ZmbM_CitizenASkinny_Blue");
            zmbClasses.Insert("ZmbM_CitizenASkinny_Brown");
            zmbClasses.Insert("ZmbM_CitizenASkinny_Grey");
            zmbClasses.Insert("ZmbM_CitizenASkinny_Red");
            zmbClasses.Insert("ZmbM_CitizenBFat_Blue");
            zmbClasses.Insert("ZmbM_CitizenBFat_Green");
            zmbClasses.Insert("ZmbM_CitizenBFat_Red");
            zmbClasses.Insert("ZmbM_ClerkFat_Brown");
            zmbClasses.Insert("ZmbM_ClerkFat_Grey");
            zmbClasses.Insert("ZmbM_ClerkFat_Khaki");
            zmbClasses.Insert("ZmbM_ClerkFat_White");
            zmbClasses.Insert("ZmbM_CommercialPilotOld_Blue");
            zmbClasses.Insert("ZmbM_CommercialPilotOld_Brown");
            zmbClasses.Insert("ZmbM_CommercialPilotOld_Grey");
            zmbClasses.Insert("ZmbM_CommercialPilotOld_Olive");
            zmbClasses.Insert("ZmbM_ConstrWorkerNormal_Beige");
            zmbClasses.Insert("ZmbM_ConstrWorkerNormal_Black");
            zmbClasses.Insert("ZmbM_ConstrWorkerNormal_Green");
            zmbClasses.Insert("ZmbM_ConstrWorkerNormal_Grey");
            zmbClasses.Insert("ZmbM_DoctorFat");
            zmbClasses.Insert("ZmbM_FarmerFat_Beige");
            zmbClasses.Insert("ZmbM_FarmerFat_Blue");
            zmbClasses.Insert("ZmbM_FarmerFat_Brown");
            zmbClasses.Insert("ZmbM_FarmerFat_Green");
            zmbClasses.Insert("ZmbM_FirefighterNormal");
            zmbClasses.Insert("ZmbM_FishermanOld_Blue");
            zmbClasses.Insert("ZmbM_FishermanOld_Green");
            zmbClasses.Insert("ZmbM_FishermanOld_Grey");
            zmbClasses.Insert("ZmbM_FishermanOld_Red");
            zmbClasses.Insert("ZmbM_HandymanNormal_Beige");
            zmbClasses.Insert("ZmbM_HandymanNormal_Blue");
            zmbClasses.Insert("ZmbM_HandymanNormal_Green");
            zmbClasses.Insert("ZmbM_HandymanNormal_Grey");
            zmbClasses.Insert("ZmbM_HandymanNormal_White");
            zmbClasses.Insert("ZmbM_HeavyIndustryWorker");
            zmbClasses.Insert("ZmbM_HermitSkinny_Beige");
            zmbClasses.Insert("ZmbM_HermitSkinny_Black");
            zmbClasses.Insert("ZmbM_HermitSkinny_Green");
            zmbClasses.Insert("ZmbM_HermitSkinny_Red");
            zmbClasses.Insert("ZmbM_HikerSkinny_Blue");
            zmbClasses.Insert("ZmbM_HikerSkinny_Green");
            zmbClasses.Insert("ZmbM_HikerSkinny_Yellow");
            zmbClasses.Insert("ZmbM_HunterOld_Autumn");
            zmbClasses.Insert("ZmbM_HunterOld_Spring");
            zmbClasses.Insert("ZmbM_HunterOld_Summer");
            zmbClasses.Insert("ZmbM_HunterOld_Winter");
            zmbClasses.Insert("ZmbM_Jacket_beige");
            zmbClasses.Insert("ZmbM_Jacket_black");
            zmbClasses.Insert("ZmbM_Jacket_blue");
            zmbClasses.Insert("ZmbM_Jacket_bluechecks");
            zmbClasses.Insert("ZmbM_Jacket_brown");
            zmbClasses.Insert("ZmbM_Jacket_greenchecks");
            zmbClasses.Insert("ZmbM_Jacket_grey");
            zmbClasses.Insert("ZmbM_Jacket_khaki");
            zmbClasses.Insert("ZmbM_Jacket_magenta");
            zmbClasses.Insert("ZmbM_Jacket_stripes");
            zmbClasses.Insert("ZmbM_JoggerSkinny_Blue");
            zmbClasses.Insert("ZmbM_JoggerSkinny_Green");
            zmbClasses.Insert("ZmbM_JoggerSkinny_Red");
            zmbClasses.Insert("ZmbM_JournalistSkinny");
            zmbClasses.Insert("ZmbM_MechanicSkinny_Blue");
            zmbClasses.Insert("ZmbM_MechanicSkinny_Green");
            zmbClasses.Insert("ZmbM_MechanicSkinny_Grey");
            zmbClasses.Insert("ZmbM_MechanicSkinny_Red");
            zmbClasses.Insert("ZmbM_MotobikerFat_Beige");
            zmbClasses.Insert("ZmbM_MotobikerFat_Black");
            zmbClasses.Insert("ZmbM_MotobikerFat_Blue");
            zmbClasses.Insert("ZmbM_OffshoreWorker_Green");
            zmbClasses.Insert("ZmbM_OffshoreWorker_Orange");
            zmbClasses.Insert("ZmbM_OffshoreWorker_Red");
            zmbClasses.Insert("ZmbM_OffshoreWorker_Yellow");
            zmbClasses.Insert("ZmbM_ParamedicNormal_Black");
            zmbClasses.Insert("ZmbM_ParamedicNormal_Blue");
            zmbClasses.Insert("ZmbM_ParamedicNormal_Green");
            zmbClasses.Insert("ZmbM_ParamedicNormal_Red");
            zmbClasses.Insert("ZmbM_PatientSkinny");
            zmbClasses.Insert("ZmbM_PatrolNormal_Autumn");
            zmbClasses.Insert("ZmbM_PatrolNormal_Flat");
            zmbClasses.Insert("ZmbM_PatrolNormal_PautRev");
            zmbClasses.Insert("ZmbM_PatrolNormal_Summer");
            zmbClasses.Insert("ZmbM_PolicemanFat");
            zmbClasses.Insert("ZmbM_PolicemanSpecForce");
            zmbClasses.Insert("ZmbM_PrisonerSkinny");
            zmbClasses.Insert("ZmbM_SkaterYoung_Blue");
            zmbClasses.Insert("ZmbM_SkaterYoung_Brown");
            zmbClasses.Insert("ZmbM_SkaterYoung_Green");
            zmbClasses.Insert("ZmbM_SkaterYoung_Grey");
            zmbClasses.Insert("ZmbM_SoldierNormal");
            zmbClasses.Insert("ZmbM_VillagerOld_Blue");
            zmbClasses.Insert("ZmbM_VillagerOld_Green");
            zmbClasses.Insert("ZmbM_VillagerOld_White");
            zmbClasses.Insert("ZmbM_priestPopSkinny");
            zmbClasses.Insert("ZmbM_usSoldier_normal_Desert");
            zmbClasses.Insert("ZmbM_usSoldier_normal_Woodland");
            return zmbClasses[Math.RandomInt(0, zmbClasses.Count())];
        }

        // Anything already a full classname passes through directly
        return entityClassName;
    }

    // Cached zombie classname pool - built once on first use
    private static ref array<string> s_zmbClassCache;

    private static void BuildZmbClassCache()
    {
        s_zmbClassCache = new array<string>();
        s_zmbClassCache.Insert("ZmbF_BlueCollarFat_Blue"); s_zmbClassCache.Insert("ZmbF_BlueCollarFat_Green");
        s_zmbClassCache.Insert("ZmbF_BlueCollarFat_Red"); s_zmbClassCache.Insert("ZmbF_BlueCollarFat_White");
        s_zmbClassCache.Insert("ZmbF_CitizenANormal_Beige"); s_zmbClassCache.Insert("ZmbF_CitizenANormal_Blue");
        s_zmbClassCache.Insert("ZmbF_CitizenANormal_Brown"); s_zmbClassCache.Insert("ZmbF_CitizenBSkinny");
        s_zmbClassCache.Insert("ZmbF_ClerkFat_Black"); s_zmbClassCache.Insert("ZmbF_ClerkFat_BluePattern");
        s_zmbClassCache.Insert("ZmbF_ClerkFat_GreyPattern"); s_zmbClassCache.Insert("ZmbF_ClerkFat_White");
        s_zmbClassCache.Insert("ZmbF_Clerk_Normal_Blue"); s_zmbClassCache.Insert("ZmbF_Clerk_Normal_Green");
        s_zmbClassCache.Insert("ZmbF_Clerk_Normal_Red"); s_zmbClassCache.Insert("ZmbF_Clerk_Normal_White");
        s_zmbClassCache.Insert("ZmbF_DoctorSkinny"); s_zmbClassCache.Insert("ZmbF_HikerSkinny_Blue");
        s_zmbClassCache.Insert("ZmbF_HikerSkinny_Green"); s_zmbClassCache.Insert("ZmbF_HikerSkinny_Grey");
        s_zmbClassCache.Insert("ZmbF_HikerSkinny_Red"); s_zmbClassCache.Insert("ZmbF_JoggerSkinny_Blue");
        s_zmbClassCache.Insert("ZmbF_JoggerSkinny_Brown"); s_zmbClassCache.Insert("ZmbF_JoggerSkinny_Green");
        s_zmbClassCache.Insert("ZmbF_JoggerSkinny_Red"); s_zmbClassCache.Insert("ZmbF_JournalistNormal_Blue");
        s_zmbClassCache.Insert("ZmbF_JournalistNormal_Green"); s_zmbClassCache.Insert("ZmbF_JournalistNormal_Red");
        s_zmbClassCache.Insert("ZmbF_JournalistNormal_White"); s_zmbClassCache.Insert("ZmbF_MechanicNormal_Beige");
        s_zmbClassCache.Insert("ZmbF_MechanicNormal_Green"); s_zmbClassCache.Insert("ZmbF_MechanicNormal_Grey");
        s_zmbClassCache.Insert("ZmbF_MechanicNormal_Orange"); s_zmbClassCache.Insert("ZmbF_MilkMaidOld_Beige");
        s_zmbClassCache.Insert("ZmbF_MilkMaidOld_Black"); s_zmbClassCache.Insert("ZmbF_MilkMaidOld_Green");
        s_zmbClassCache.Insert("ZmbF_MilkMaidOld_Grey"); s_zmbClassCache.Insert("ZmbF_NurseFat");
        s_zmbClassCache.Insert("ZmbF_ParamedicNormal_Blue"); s_zmbClassCache.Insert("ZmbF_ParamedicNormal_Green");
        s_zmbClassCache.Insert("ZmbF_ParamedicNormal_Red"); s_zmbClassCache.Insert("ZmbF_PatientOld");
        s_zmbClassCache.Insert("ZmbF_PoliceWomanNormal"); s_zmbClassCache.Insert("ZmbF_ShortSkirt_beige");
        s_zmbClassCache.Insert("ZmbF_ShortSkirt_black"); s_zmbClassCache.Insert("ZmbF_ShortSkirt_brown");
        s_zmbClassCache.Insert("ZmbF_ShortSkirt_checks"); s_zmbClassCache.Insert("ZmbF_ShortSkirt_green");
        s_zmbClassCache.Insert("ZmbF_ShortSkirt_grey"); s_zmbClassCache.Insert("ZmbF_ShortSkirt_red");
        s_zmbClassCache.Insert("ZmbF_ShortSkirt_stripes"); s_zmbClassCache.Insert("ZmbF_ShortSkirt_white");
        s_zmbClassCache.Insert("ZmbF_ShortSkirt_yellow"); s_zmbClassCache.Insert("ZmbF_SkaterYoung_Brown");
        s_zmbClassCache.Insert("ZmbF_SkaterYoung_Striped"); s_zmbClassCache.Insert("ZmbF_SkaterYoung_Violet");
        s_zmbClassCache.Insert("ZmbF_SurvivorNormal_Blue"); s_zmbClassCache.Insert("ZmbF_SurvivorNormal_Orange");
        s_zmbClassCache.Insert("ZmbF_SurvivorNormal_Red"); s_zmbClassCache.Insert("ZmbF_SurvivorNormal_White");
        s_zmbClassCache.Insert("ZmbF_VillagerOld_Blue"); s_zmbClassCache.Insert("ZmbF_VillagerOld_Green");
        s_zmbClassCache.Insert("ZmbF_VillagerOld_Red"); s_zmbClassCache.Insert("ZmbF_VillagerOld_White");
        s_zmbClassCache.Insert("ZmbM_CitizenASkinny_Blue"); s_zmbClassCache.Insert("ZmbM_CitizenASkinny_Brown");
        s_zmbClassCache.Insert("ZmbM_CitizenASkinny_Grey"); s_zmbClassCache.Insert("ZmbM_CitizenASkinny_Red");
        s_zmbClassCache.Insert("ZmbM_CitizenBFat_Blue"); s_zmbClassCache.Insert("ZmbM_CitizenBFat_Green");
        s_zmbClassCache.Insert("ZmbM_CitizenBFat_Red"); s_zmbClassCache.Insert("ZmbM_ClerkFat_Brown");
        s_zmbClassCache.Insert("ZmbM_ClerkFat_Grey"); s_zmbClassCache.Insert("ZmbM_ClerkFat_Khaki");
        s_zmbClassCache.Insert("ZmbM_ClerkFat_White"); s_zmbClassCache.Insert("ZmbM_CommercialPilotOld_Blue");
        s_zmbClassCache.Insert("ZmbM_CommercialPilotOld_Brown"); s_zmbClassCache.Insert("ZmbM_CommercialPilotOld_Grey");
        s_zmbClassCache.Insert("ZmbM_CommercialPilotOld_Olive"); s_zmbClassCache.Insert("ZmbM_ConstrWorkerNormal_Beige");
        s_zmbClassCache.Insert("ZmbM_ConstrWorkerNormal_Black"); s_zmbClassCache.Insert("ZmbM_ConstrWorkerNormal_Green");
        s_zmbClassCache.Insert("ZmbM_ConstrWorkerNormal_Grey"); s_zmbClassCache.Insert("ZmbM_DoctorFat");
        s_zmbClassCache.Insert("ZmbM_FarmerFat_Beige"); s_zmbClassCache.Insert("ZmbM_FarmerFat_Blue");
        s_zmbClassCache.Insert("ZmbM_FarmerFat_Brown"); s_zmbClassCache.Insert("ZmbM_FarmerFat_Green");
        s_zmbClassCache.Insert("ZmbM_FirefighterNormal"); s_zmbClassCache.Insert("ZmbM_FishermanOld_Blue");
        s_zmbClassCache.Insert("ZmbM_FishermanOld_Green"); s_zmbClassCache.Insert("ZmbM_FishermanOld_Grey");
        s_zmbClassCache.Insert("ZmbM_FishermanOld_Red"); s_zmbClassCache.Insert("ZmbM_HandymanNormal_Beige");
        s_zmbClassCache.Insert("ZmbM_HandymanNormal_Blue"); s_zmbClassCache.Insert("ZmbM_HandymanNormal_Green");
        s_zmbClassCache.Insert("ZmbM_HandymanNormal_Grey"); s_zmbClassCache.Insert("ZmbM_HandymanNormal_White");
        s_zmbClassCache.Insert("ZmbM_HeavyIndustryWorker"); s_zmbClassCache.Insert("ZmbM_HermitSkinny_Beige");
        s_zmbClassCache.Insert("ZmbM_HermitSkinny_Black"); s_zmbClassCache.Insert("ZmbM_HermitSkinny_Green");
        s_zmbClassCache.Insert("ZmbM_HermitSkinny_Red"); s_zmbClassCache.Insert("ZmbM_HikerSkinny_Blue");
        s_zmbClassCache.Insert("ZmbM_HikerSkinny_Green"); s_zmbClassCache.Insert("ZmbM_HikerSkinny_Yellow");
        s_zmbClassCache.Insert("ZmbM_HunterOld_Autumn"); s_zmbClassCache.Insert("ZmbM_HunterOld_Spring");
        s_zmbClassCache.Insert("ZmbM_HunterOld_Summer"); s_zmbClassCache.Insert("ZmbM_HunterOld_Winter");
        s_zmbClassCache.Insert("ZmbM_Jacket_beige"); s_zmbClassCache.Insert("ZmbM_Jacket_black");
        s_zmbClassCache.Insert("ZmbM_Jacket_blue"); s_zmbClassCache.Insert("ZmbM_Jacket_bluechecks");
        s_zmbClassCache.Insert("ZmbM_Jacket_brown"); s_zmbClassCache.Insert("ZmbM_Jacket_greenchecks");
        s_zmbClassCache.Insert("ZmbM_Jacket_grey"); s_zmbClassCache.Insert("ZmbM_Jacket_khaki");
        s_zmbClassCache.Insert("ZmbM_Jacket_magenta"); s_zmbClassCache.Insert("ZmbM_Jacket_stripes");
        s_zmbClassCache.Insert("ZmbM_JoggerSkinny_Blue"); s_zmbClassCache.Insert("ZmbM_JoggerSkinny_Green");
        s_zmbClassCache.Insert("ZmbM_JoggerSkinny_Red"); s_zmbClassCache.Insert("ZmbM_JournalistSkinny");
        s_zmbClassCache.Insert("ZmbM_MechanicSkinny_Blue"); s_zmbClassCache.Insert("ZmbM_MechanicSkinny_Green");
        s_zmbClassCache.Insert("ZmbM_MechanicSkinny_Grey"); s_zmbClassCache.Insert("ZmbM_MechanicSkinny_Red");
        s_zmbClassCache.Insert("ZmbM_MotobikerFat_Beige"); s_zmbClassCache.Insert("ZmbM_MotobikerFat_Black");
        s_zmbClassCache.Insert("ZmbM_MotobikerFat_Blue"); s_zmbClassCache.Insert("ZmbM_OffshoreWorker_Green");
        s_zmbClassCache.Insert("ZmbM_OffshoreWorker_Orange"); s_zmbClassCache.Insert("ZmbM_OffshoreWorker_Red");
        s_zmbClassCache.Insert("ZmbM_OffshoreWorker_Yellow"); s_zmbClassCache.Insert("ZmbM_ParamedicNormal_Black");
        s_zmbClassCache.Insert("ZmbM_ParamedicNormal_Blue"); s_zmbClassCache.Insert("ZmbM_ParamedicNormal_Green");
        s_zmbClassCache.Insert("ZmbM_ParamedicNormal_Red"); s_zmbClassCache.Insert("ZmbM_PatientSkinny");
        s_zmbClassCache.Insert("ZmbM_PatrolNormal_Autumn"); s_zmbClassCache.Insert("ZmbM_PatrolNormal_Flat");
        s_zmbClassCache.Insert("ZmbM_PatrolNormal_PautRev"); s_zmbClassCache.Insert("ZmbM_PatrolNormal_Summer");
        s_zmbClassCache.Insert("ZmbM_PolicemanFat"); s_zmbClassCache.Insert("ZmbM_PolicemanSpecForce");
        s_zmbClassCache.Insert("ZmbM_PrisonerSkinny"); s_zmbClassCache.Insert("ZmbM_SkaterYoung_Blue");
        s_zmbClassCache.Insert("ZmbM_SkaterYoung_Brown"); s_zmbClassCache.Insert("ZmbM_SkaterYoung_Green");
        s_zmbClassCache.Insert("ZmbM_SkaterYoung_Grey"); s_zmbClassCache.Insert("ZmbM_SoldierNormal");
        s_zmbClassCache.Insert("ZmbM_VillagerOld_Blue"); s_zmbClassCache.Insert("ZmbM_VillagerOld_Green");
        s_zmbClassCache.Insert("ZmbM_VillagerOld_White"); s_zmbClassCache.Insert("ZmbM_priestPopSkinny");
        s_zmbClassCache.Insert("ZmbM_usSoldier_normal_Desert"); s_zmbClassCache.Insert("ZmbM_usSoldier_normal_Woodland");
    }

    // Returns true if the classname is a ManType entity (infected/player subclass)
    // ManType cannot use spawnOnGround=true - must use false + manual Y position
    private static bool IsManTypeEntity(string className)
    {
        if (className.Length() < 4) return false;
        string prefix4 = className.Substring(0, 4);
        return (prefix4 == "ZmbM" || prefix4 == "ZmbF" || prefix4 == "Surv" || prefix4 == "Dayo");
    }

    private void SpawnQuestEntities(PlayerBase player, QT_QuestDef def)
    {
        string uid = player.GetIdentity().GetId();
        string key = uid + "_" + def.id;
        vector playerPos = player.GetPosition();

        // Clear any previously spawned entities for this quest before spawning new ones
        // This prevents cancel-reaccept loops from building up hordes
        if (m_spawnedQuestEntities.Contains(key))
        {
            array<Object> oldEntities = m_spawnedQuestEntities.Get(key);
            foreach (Object oldEnt : oldEntities)
            {
                if (oldEnt)
                {
                    EntityAI oldEntAI = EntityAI.Cast(oldEnt);
                    if (oldEntAI && oldEntAI.IsAlive())
                        GetGame().ObjectDelete(oldEnt);
                }
            }
            m_spawnedQuestEntities.Remove(key);
            Print("[QuestTrader] Cleared previous spawned entities for quest: " + def.id + " player: " + uid);
        }

        ref array<Object> spawnedEntities = new array<Object>();

        foreach (QT_Objective obj : def.objectives)
        {
            string spawnClass = ResolveSpawnClass(obj.entityClassName);

            // Empty string means this is a dummy/recon objective — skip spawning
            if (spawnClass == "") continue;

            vector spawnCenter;
            if (def.killTargetSpawnPosition != vector.Zero)
            {
                spawnCenter = def.killTargetSpawnPosition;
                if (spawnCenter[1] == 0)
                    spawnCenter[1] = GetGame().SurfaceY(spawnCenter[0], spawnCenter[2]);
            }
            else
            {
                // No fixed position set - spawn 200-250m from player
                float angle = Math.RandomFloat(0, Math.PI2);
                float dist  = Math.RandomFloat(200, 250);
                spawnCenter[0] = playerPos[0] + Math.Sin(angle) * dist;
                spawnCenter[2] = playerPos[2] + Math.Cos(angle) * dist;
                spawnCenter[1] = GetGame().SurfaceY(spawnCenter[0], spawnCenter[2]);
            }

            float spawnRadius = def.killTargetSpawnRadius;
            if (spawnRadius <= 0) spawnRadius = 20.0;

            for (int i = 0; i < obj.requiredAmount; i++)
            {
                // Try up to 5 positions, pick one that has open sky above it
                vector spawnPos = spawnCenter;
                for (int attempt = 0; attempt < 5; attempt++)
                {
                    float scatter = Math.RandomFloat(0, Math.PI2);
                    float scatterDist = Math.RandomFloat(0, spawnRadius);
                    vector candidate;
                    candidate[0] = spawnCenter[0] + Math.Sin(scatter) * scatterDist;
                    candidate[2] = spawnCenter[2] + Math.Cos(scatter) * scatterDist;
                    candidate[1] = GetGame().SurfaceY(candidate[0], candidate[2]) + 0.5;

                    // Raycast upward — if nothing is hit within 3m we're not inside a building
                    vector rayStart = candidate;
                    vector rayEnd   = candidate + Vector(0, 3, 0);
                    vector hitPos, hitNormal;
                    float hitFraction;
                    Object hitObject;
                    PhxInteractionLayers collisionLayer = PhxInteractionLayers.BUILDING;
                    if (!DayZPhysics.RayCastBullet(rayStart, rayEnd, collisionLayer, null, hitObject, hitPos, hitNormal, hitFraction))
                    {
                        spawnPos = candidate;
                        break;
                    }
                }

                string resolvedClass = ResolveSpawnClass(obj.entityClassName);
                bool isManType = IsManTypeEntity(resolvedClass);
                // ManType entities (infected) must use spawnOnGround=false to avoid
                // "ManType must inherit from DayZAnimalType" engine error.
                // Animals use spawnOnGround=true for correct AI and physics init,
                // but we do NOT register them with the AI world population counter
                // by using CreateObject instead of CreateObjectEx.
                if (isManType)
                    spawnPos[1] = GetGame().SurfaceY(spawnPos[0], spawnPos[2]) + 0.1;
                Object spawned = GetGame().CreateObject(resolvedClass, spawnPos, false, !isManType);
                if (spawned) spawnedEntities.Insert(spawned);
            }

            Print("[QuestTrader] Spawned " + obj.requiredAmount + "x " + spawnClass + " for quest: " + def.title);
        }

        // Track all spawned entities for this player+quest
        if (spawnedEntities.Count() > 0)
            m_spawnedQuestEntities.Insert(key, spawnedEntities);
    }

    void CompleteReconObjective(PlayerBase player, string questId)
    {
        if (!player || !player.GetIdentity()) return;
        string uid = player.GetIdentity().GetId();

        auto playerMap = GetOrCreatePlayerMap(uid);
        if (!playerMap.Contains(questId)) return;

        QT_PlayerQuestState qs = playerMap.Get(questId);
        if (!qs || qs.state != QT_QuestState.ACTIVE) return;

        QT_QuestDef def = GetQuestDef(questId);
        if (!def) return;

        // Mark all objectives complete
        for (int i = 0; i < qs.objectiveProgress.Count(); i++)
        {
            QT_Objective obj = def.objectives[i];
            qs.objectiveProgress.Set(i, obj.requiredAmount);
        }

        qs.state = QT_QuestState.COMPLETED;
        MarkPlayerDirty(uid);

        if (questId == "quest_201")
        {
            SpawnHelicopterCrash("quest_201");
            QT_RPCManager.SendToast(player, "#QuestTrader_QUEST_READY_TURN_IN: " + def.title, QT_ToastType.COMPLETE);
        }
        else
        {
            SpawnHelicopterCrash("quest_179");
            QT_RPCManager.SendToast(player, "#QuestTrader_QUEST_READY_TURN_IN: " + def.title, QT_ToastType.COMPLETE);
        }

        QT_RPCManager.SendHUDUpdate(player);
        Print("[QuestTrader] Recon objective completed for quest: " + questId + " player: " + uid);
    }

    private void SpawnHelicopterCrash(string questId = "quest_201")
    {
        vector crashPos;
        if (questId == "quest_179")
            crashPos = Vector(14030.4, 4.15, 11181.0);
        else
            crashPos = Vector(4569.39, 340.8, 10402.5);

        // Play crash sound immediately
        string soundSet = "HeliCrash_Distant_SoundSet";
        Param3<bool, vector, int> playSound = new Param3<bool, vector, int>(true, crashPos, soundSet.Hash());
        g_Game.RPCSingleParam(null, ERPCs.RPC_SOUND_HELICRASH, playSound, true);
        Print("[QuestTrader] Played helicopter crash sound for " + questId);

        // Delay the wreck and loot spawn by 20 seconds
        if (questId == "quest_179")
            GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(SpawnHelicopterWreck179, 20000, false);
        else
            GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(SpawnHelicopterWreck, 20000, false);
    }

    private void SpawnHelicopterWreck179()
    {
        vector crashPos = Vector(14030.4, 4.15, 11181.0);

        Object heli = GetGame().CreateObject("Wreck_Mi8_Crashed", crashPos, false, true);
        if (heli)
            Print("[QuestTrader] Spawned Wreck_Mi8_Crashed at Rify coast");
        else
            Print("[QuestTrader] WARNING: Failed to spawn Wreck_Mi8_Crashed at Rify coast");

        // Civilian weapons and ammo loot
        ref array<string> loot179 = {
            "Mosin9130", "Winchester70", "Ruger1022", "SKS", "CZ75", "Glock19",
            "AmmoBox_762x54_20Rnd", "AmmoBox_762x54_20Rnd",
            "AmmoBox_308Win_20Rnd", "AmmoBox_308Win_20Rnd",
            "AmmoBox_22_50Rnd", "AmmoBox_22_50Rnd",
            "AmmoBox_9x19_25rnd", "AmmoBox_9x19_25rnd",
            "AmmoBox_12gaSlug_10Rnd",
            "Mag_Ruger1022_30Rnd", "Mag_Ruger1022_30Rnd",
            "HuntingOptic", "HuntingKnife", "PetrolLighter"
        };

        foreach (string item : loot179)
        {
            float angle = Math.RandomFloat(0, Math.PI2);
            float dist  = Math.RandomFloat(3, 12);
            vector lootPos;
            lootPos[0] = crashPos[0] + Math.Sin(angle) * dist;
            lootPos[2] = crashPos[2] + Math.Cos(angle) * dist;
            lootPos[1] = GetGame().SurfaceY(lootPos[0], lootPos[2]) + 0.3;
            GetGame().CreateObject(item, lootPos, false, true);
        }

        Print("[QuestTrader] Spawned civilian weapons loot at Rify crash site");

        // Spawn civilian zombies around the crash site
        ref array<string> zombies179 = {"ZmbF_JoggerSkinny_Red", "ZmbF_ShortSkirt_stripes", "ZmbM_PolicemanSpecForce"};
        for (int z179 = 0; z179 < 4; z179++)
        {
            float zAngle2 = Math.RandomFloat(0, Math.PI2);
            float zDist2  = Math.RandomFloat(5, 15);
            vector zPos2;
            zPos2[0] = crashPos[0] + Math.Sin(zAngle2) * zDist2;
            zPos2[2] = crashPos[2] + Math.Cos(zAngle2) * zDist2;
            zPos2[1] = GetGame().SurfaceY(zPos2[0], zPos2[2]);
            GetGame().CreateObject(zombies179[Math.RandomInt(0, zombies179.Count())], zPos2, false, true);
        }
        Print("[QuestTrader] Spawned civilian zombies at Rify crash site");
    }

    private void SpawnHelicopterWreck()
    {
        vector crashPos = Vector(4569.39, 340.8, 10402.5);

        // Spawn the Mi-8 wreck
        Object heli = GetGame().CreateObject("Wreck_Mi8_Crashed", crashPos, false, true);
        if (heli)
            Print("[QuestTrader] Spawned Wreck_Mi8_Crashed at " + crashPos.ToString());
        else
            Print("[QuestTrader] WARNING: Failed to spawn Wreck_Mi8_Crashed");

        // Spawn smoke grenades at crash site for visual effect
        for (int s = 0; s < 5; s++)
        {
            float sAngle = Math.RandomFloat(0, Math.PI2);
            float sDist  = Math.RandomFloat(0, 8);
            vector smokePos;
            smokePos[0] = crashPos[0] + Math.Sin(sAngle) * sDist;
            smokePos[2] = crashPos[2] + Math.Cos(sAngle) * sDist;
            smokePos[1] = GetGame().SurfaceY(smokePos[0], smokePos[2]) + 0.3;
            GetGame().CreateObject("RDG2SmokeGrenade_Black", smokePos, false, true);
        }

        // Spawn medical loot scattered around the crash site
        ref array<string> loot = {
            "SalineBag", "SalineBag", "SalineBag",
            "Morphine", "Morphine", "Morphine", "Morphine",
            "Epinephrine", "Epinephrine",
            "BandageDressing", "BandageDressing", "BandageDressing", "BandageDressing", "BandageDressing",
            "TetracyclineAntibiotics", "TetracyclineAntibiotics",
            "BloodBagEmpty", "BloodBagEmpty",
            "Splint", "Splint",
            "DisinfectantSpray",
            "SewingKit"
        };

        foreach (string item : loot)
        {
            // Scatter loot 3-12m from crash centre
            float angle = Math.RandomFloat(0, Math.PI2);
            float dist  = Math.RandomFloat(3, 12);
            vector lootPos;
            lootPos[0] = crashPos[0] + Math.Sin(angle) * dist;
            lootPos[2] = crashPos[2] + Math.Cos(angle) * dist;
            lootPos[1] = GetGame().SurfaceY(lootPos[0], lootPos[2]) + 0.3;

            GetGame().CreateObject(item, lootPos, false, true);
        }

        Print("[QuestTrader] Spawned helicopter crash medical loot at NWAF");

        // Spawn medical zombies around the crash site
        ref array<string> zombies201 = {"ZmbF_DoctorSkinny", "ZmbM_ParamedicNormal_Green", "ZmbM_DoctorFat"};
        for (int z201 = 0; z201 < 4; z201++)
        {
            float zAngle = Math.RandomFloat(0, Math.PI2);
            float zDist  = Math.RandomFloat(5, 15);
            vector zPos;
            zPos[0] = crashPos[0] + Math.Sin(zAngle) * zDist;
            zPos[2] = crashPos[2] + Math.Cos(zAngle) * zDist;
            zPos[1] = GetGame().SurfaceY(zPos[0], zPos[2]);
            GetGame().CreateObject(zombies201[Math.RandomInt(0, zombies201.Count())], zPos, false, true);
        }
        Print("[QuestTrader] Spawned medical zombies at NWAF crash site");
    }

    bool CancelQuest(PlayerBase player, string questId)
    {
        if (!player) return false;
        string uid  = player.GetIdentity().GetId();
        string name = player.GetIdentity().GetName();

        auto qs = GetPlayerQuestState(uid, questId);
        if (qs.state != QT_QuestState.ACTIVE && qs.state != QT_QuestState.COMPLETED && qs.state != QT_QuestState.COOLDOWN)
        {
            QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_NO_ACTIVE_CANCEL", QT_ToastType.WARNING);
            return false;
        }

        // Reset state and all progress
        qs.state = QT_QuestState.AVAILABLE;
        qs.completedTimestamp = 0;
        for (int ci = 0; ci < qs.objectiveProgress.Count(); ci++)
            qs.objectiveProgress.Set(ci, 0);

        QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_QUEST_CANCELLED", QT_ToastType.WARNING);
        QT_RPCManager.SendHUDUpdate(player);
        QT_Logger.GetInstance().Info("QUEST", "Cancelled: " + questId, uid, name);
        CleanupQuestItem(uid, questId);

        // Remove delivery item from inventory if this was a deliver quest
        QT_QuestDef def = GetQuestDef(questId);
        if (def && def.type == QT_QuestType.DELIVER)
            RemoveDeliveryItem(player, def);

        MarkPlayerDirty(uid);

        return true;
    }

    // Give player the delivery item when they accept a DELIVER quest
    private void GiveDeliveryItem(PlayerBase player, QT_QuestDef def)
    {
        if (def.deliveryItemClass == "") return;
        player.GetInventory().CreateInInventory(def.deliveryItemClass);
        Print("[QuestTrader] Gave delivery item: " + def.deliveryItemClass + " to " + player.GetIdentity().GetName());
    }

    // Remove delivery item from player inventory
    private bool HasDeliveryItem(PlayerBase player, QT_QuestDef def)
    {
        if (def.deliveryItemClass == "") return true;
        array<EntityAI> items = new array<EntityAI>();
        player.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
        foreach (EntityAI item : items)
        {
            if (item.GetType() == def.deliveryItemClass || item.IsKindOf(def.deliveryItemClass))
                return true;
        }
        return false;
    }

    private void RemoveDeliveryItem(PlayerBase player, QT_QuestDef def)
    {
        if (def.deliveryItemClass == "") return;
        array<EntityAI> items = new array<EntityAI>();
        player.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
        foreach (EntityAI item : items)
        {
            if (item.GetType() == def.deliveryItemClass || item.IsKindOf(def.deliveryItemClass))
            {
                GetGame().ObjectDelete(item);
                return;
            }
        }
    }

    bool TurnInDeliverQuest(PlayerBase player, string questId)
    {
        if (!player) return false;
        string uid  = player.GetIdentity().GetId();
        string name = player.GetIdentity().GetName();

        QT_QuestDef def = GetQuestDef(questId);
        if (!def || def.type != QT_QuestType.DELIVER) return false;

        auto qs = GetPlayerQuestState(uid, questId);
        if (qs.state != QT_QuestState.ACTIVE && qs.state != QT_QuestState.COMPLETED)
        {
            QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_NOT_ACCEPTED", QT_ToastType.WARNING);
            return false;
        }

        // For delivery: mark COMPLETED once player has item and turns in
        if (!HasDeliveryItem(player, def))
        {
            QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_LOST_DELIVERY_ITEM", QT_ToastType.WARNING);
            return false;
        }

        RemoveDeliveryItem(player, def);
        GiveRewards(player, def);
        CleanupQuestItem(uid, questId);
        RecordCompletion(player, def);

        if (def.repeatable) qs.state = QT_QuestState.COOLDOWN;
        else                qs.state = QT_QuestState.TURNED_IN;
        qs.completedTimestamp = GetGame().GetTime() / 1000;

        SavePlayerImmediate(uid, "quest reward delivered");
        if (def.rewardMessage != "")
            QT_RPCManager.SendToast(player, def.rewardMessage, QT_ToastType.REWARD);
        SendQuestCompleteNotification(player, def);
        QT_RPCManager.SendHUDUpdate(player);
        QT_Logger.GetInstance().Info("QUEST", "Completed (deliver): " + def.title, uid, name);
        return true;
    }

    bool TurnInCollectQuest(PlayerBase player, string questId)
    {
        if (!player) return false;
        string uid  = player.GetIdentity().GetId();
        string name = player.GetIdentity().GetName();

        QT_QuestDef def = GetQuestDef(questId);
        if (!def || def.type != QT_QuestType.COLLECT) return false;

        auto qs = GetPlayerQuestState(uid, questId);
        if (qs.state != QT_QuestState.ACTIVE && qs.state != QT_QuestState.COMPLETED)
        {
            QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_NOT_ACCEPTED", QT_ToastType.WARNING);
            return false;
        }

        foreach (QT_Objective obj : def.objectives)
        {
            int count = CountItemsInInventory(player, obj.itemClassName);
            if (count < obj.requiredAmount)
            {
                int need = obj.requiredAmount - count;
                QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_STILL_NEED " + need + "x " + obj.itemClassName, QT_ToastType.WARNING);
                return false;
            }
        }

        foreach (QT_Objective obj2 : def.objectives)
            RemoveItemsFromInventory(player, obj2.itemClassName, obj2.requiredAmount);

        GiveRewards(player, def);
        CleanupQuestItem(uid, questId);
        RecordCompletion(player, def);

        if (def.repeatable) qs.state = QT_QuestState.COOLDOWN;
        else                qs.state = QT_QuestState.TURNED_IN;
        qs.completedTimestamp = GetGame().GetTime() / 1000;

        SavePlayerImmediate(uid, "quest reward delivered");
        if (def.rewardMessage != "")
            QT_RPCManager.SendToast(player, def.rewardMessage, QT_ToastType.REWARD);
        SendQuestCompleteNotification(player, def);
        QT_RPCManager.SendHUDUpdate(player);
        QT_Logger.GetInstance().Info("QUEST", "Completed (collect): " + def.title, uid, name);
        return true;
    }

    bool TurnInKillQuest(PlayerBase player, string questId)
    {
        if (!player) return false;
        string uid  = player.GetIdentity().GetId();
        string name = player.GetIdentity().GetName();

        QT_QuestDef def = GetQuestDef(questId);
        if (!def || def.type != QT_QuestType.KILL) return false;

        auto qs = GetPlayerQuestState(uid, questId);
        if (qs.state != QT_QuestState.COMPLETED)
        {
            string prog = "#QuestTrader_TOAST_INCOMPLETE:";
            foreach (int i, QT_Objective obj : def.objectives)
            {
                int cur = 0;
                if (i < qs.objectiveProgress.Count()) cur = qs.objectiveProgress[i];
                prog = prog + " ";
                prog = prog + GetObjectiveKillLabel(obj);
                prog = prog + " ";
                prog = prog + cur.ToString();
                prog = prog + "/";
                prog = prog + obj.requiredAmount.ToString();
            }
            QT_RPCManager.SendToast(player, prog, QT_ToastType.WARNING);
            return false;
        }

        GiveRewards(player, def);
        CleanupQuestItem(uid, questId);
        RecordCompletion(player, def);

        if (def.repeatable) qs.state = QT_QuestState.COOLDOWN;
        else                qs.state = QT_QuestState.TURNED_IN;
        qs.completedTimestamp = GetGame().GetTime() / 1000;

        SavePlayerImmediate(uid, "quest reward delivered");
        if (def.rewardMessage != "")
            QT_RPCManager.SendToast(player, def.rewardMessage, QT_ToastType.REWARD);
        SendQuestCompleteNotification(player, def);
        QT_RPCManager.SendHUDUpdate(player);
        QT_Logger.GetInstance().Info("QUEST", "Completed (kill): " + def.title, uid, name);
        return true;
    }

    bool TurnInInteractionQuest(PlayerBase player, string questId)
    {
        if (!player) return false;
        string uid  = player.GetIdentity().GetId();
        string name = player.GetIdentity().GetName();

        QT_QuestDef def = GetQuestDef(questId);
        if (!def || def.type != QT_QuestType.INTERACT) return false;

        auto qs = GetPlayerQuestState(uid, questId);
        if (qs.state != QT_QuestState.COMPLETED)
        {
            QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_INTERACTION_REQUIRED", QT_ToastType.WARNING);
            return false;
        }

        GiveRewards(player, def);
        CleanupQuestItem(uid, questId);
        RecordCompletion(player, def);

        if (def.repeatable) qs.state = QT_QuestState.COOLDOWN;
        else                qs.state = QT_QuestState.TURNED_IN;
        qs.completedTimestamp = GetGame().GetTime() / 1000;

        SavePlayerImmediate(uid, "quest reward delivered");
        if (def.rewardMessage != "")
            QT_RPCManager.SendToast(player, def.rewardMessage, QT_ToastType.REWARD);
        SendQuestCompleteNotification(player, def);
        QT_RPCManager.SendHUDUpdate(player);
        QT_Logger.GetInstance().Info("QUEST", "Completed (interaction): " + def.title, uid, name);
        return true;
    }

    bool CompleteInteractionObjective(PlayerBase player, string questId, string objectClassName, vector objectPosition)
    {
        if (!GetGame().IsServer()) return false;
        if (!player || !player.GetIdentity()) return false;

        string uid = player.GetIdentity().GetId();
        auto playerMap = GetOrCreatePlayerMap(uid);
        if (!playerMap.Contains(questId)) return false;

        QT_PlayerQuestState qs = playerMap.Get(questId);
        if (!qs || qs.state != QT_QuestState.ACTIVE) return false;

        QT_QuestDef def = GetQuestDef(questId);
        if (!def || def.type != QT_QuestType.INTERACT) return false;

        if (!IsInteractionTargetValid(player, def, objectClassName, objectPosition, null))
        {
            QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_WRONG_INTERACTION_OBJECT", QT_ToastType.WARNING);
            return false;
        }

        return CompleteInteractionQuestState(player, def, qs, playerMap);
    }

    bool CompleteInteractionObjectiveFromObject(PlayerBase player, Object targetObject)
    {
        if (!GetGame().IsServer()) return false;
        if (!player || !player.GetIdentity()) return false;
        if (!targetObject) return false;

        string uid = player.GetIdentity().GetId();
        auto playerMap = GetOrCreatePlayerMap(uid);
        foreach (string questId, QT_PlayerQuestState qs : playerMap)
        {
            if (!qs || qs.state != QT_QuestState.ACTIVE) continue;

            QT_QuestDef def = GetQuestDef(questId);
            if (!def || def.type != QT_QuestType.INTERACT) continue;

            if (!IsInteractionTargetValid(player, def, targetObject.GetType(), targetObject.GetPosition(), targetObject))
                continue;

            return CompleteInteractionQuestState(player, def, qs, playerMap);
        }
        return false;
    }

    private bool IsInteractionTargetValid(PlayerBase player, QT_QuestDef def, string objectClassName, vector objectPosition, Object directObject)
    {
        if (!player || !def) return false;

        float allowedDist = def.interactionDistance;
        if (allowedDist <= 0) allowedDist = 3.0;
        allowedDist = allowedDist + 1.0;

        bool validPosition = false;
        if (def.interactionPosition != vector.Zero)
        {
            validPosition = vector.Distance(player.GetPosition(), def.interactionPosition) <= allowedDist;
        }
        else if (objectPosition != vector.Zero)
        {
            validPosition = vector.Distance(player.GetPosition(), objectPosition) <= allowedDist;
        }
        else if (directObject)
        {
            validPosition = vector.Distance(player.GetPosition(), directObject.GetPosition()) <= allowedDist;
        }

        bool validClass = true;
        if (def.interactionObjectClassName != "")
        {
            validClass = false;
            if (directObject)
            {
                string directType = directObject.GetType();
                if (directType != "" && (directType.Contains(def.interactionObjectClassName) || GetGame().IsKindOf(directType, def.interactionObjectClassName)))
                {
                    if (def.interactionPosition == vector.Zero || vector.Distance(directObject.GetPosition(), def.interactionPosition) <= allowedDist)
                        validClass = true;
                }
            }
            else
            {
                array<Object> foundObjects = new array<Object>();
                array<CargoBase> proxyCargos = new array<CargoBase>();
                GetGame().GetObjectsAtPosition(player.GetPosition(), allowedDist, foundObjects, proxyCargos);

                foreach (Object foundObj : foundObjects)
                {
                    if (!foundObj) continue;

                    string foundType = foundObj.GetType();
                    if (foundType == "") continue;
                    if (!foundType.Contains(def.interactionObjectClassName) && !GetGame().IsKindOf(foundType, def.interactionObjectClassName)) continue;

                    if (def.interactionPosition != vector.Zero && vector.Distance(foundObj.GetPosition(), def.interactionPosition) > allowedDist) continue;

                    validClass = true;
                    break;
                }
            }
        }

        return validPosition && validClass;
    }

    private bool CompleteInteractionQuestState(PlayerBase player, QT_QuestDef def, QT_PlayerQuestState qs, map<string, ref QT_PlayerQuestState> playerMap)
    {
        if (!player || !def || !qs || !playerMap) return false;

        string uid = player.GetIdentity().GetId();
        EnsureObjectiveProgress(qs, def);
        if (qs.objectiveProgress.Count() > 0)
            qs.objectiveProgress.Set(0, 1);

        qs.state = QT_QuestState.COMPLETED;
        MarkPlayerDirty(uid);
        QT_RPCManager.SendToast(player, "#QuestTrader_QUEST_READY_TURN_IN: " + def.title, QT_ToastType.COMPLETE);
        QT_RPCManager.SendHUDUpdate(player);
        QT_Logger.GetInstance().Info("QUEST", "Interaction objective completed: " + def.title, uid, player.GetIdentity().GetName());
        return true;
    }

    private bool IsKillDebugEnabled()
    {
        if (!m_config || !m_config.Settings) return false;
        return m_config.Settings.EnableKillDebug;
    }

    private void KillDebug(string msg, PlayerBase player = null)
    {
        if (!IsKillDebugEnabled()) return;

        string uid = "";
        string name = "";
        if (player && player.GetIdentity())
        {
            uid = player.GetIdentity().GetId();
            name = player.GetIdentity().GetName();
        }

        QT_Logger.GetInstance().Debug("KILL", msg, uid, name);
    }

    private string BoolText(bool value)
    {
        if (value) return "TRUE";
        return "FALSE";
    }

    private void AddKillTargetCandidate(array<string> targets, string target)
    {
        if (!targets) return;

        target = target.Trim();
        if (target == "") return;

        foreach (string existing : targets)
        {
            if (existing == target) return;
        }

        targets.Insert(target);
    }

    private void ExpandKillTarget(array<string> targets, string target)
    {
        target = target.Trim();
        if (target == "") return;

        AddKillTargetCandidate(targets, target);

        if (target == "Deer" && m_itemSettings && m_itemSettings.DeerKillAliases)
        {
            foreach (string deerAlias : m_itemSettings.DeerKillAliases)
                AddKillTargetCandidate(targets, deerAlias);
        }

        if (m_itemSettings && m_itemSettings.KillTargetGroups && m_itemSettings.KillTargetGroups.Contains(target))
        {
            array<string> groupTargets = m_itemSettings.KillTargetGroups.Get(target);
            if (groupTargets)
            {
                foreach (string groupTarget : groupTargets)
                    AddKillTargetCandidate(targets, groupTarget);
            }
        }
        else if (m_itemSettings && m_itemSettings.KillTargetGroups)
        {
            string targetLower = target;
            targetLower.ToLower();
            foreach (string groupName, array<string> fallbackGroupTargets : m_itemSettings.KillTargetGroups)
            {
                string groupLower = groupName;
                groupLower.ToLower();
                if (groupLower != targetLower) continue;

                if (fallbackGroupTargets)
                {
                    foreach (string fallbackGroupTarget : fallbackGroupTargets)
                        AddKillTargetCandidate(targets, fallbackGroupTarget);
                }
                break;
            }
        }
    }

    private void BuildObjectiveKillTargets(QT_Objective obj, array<string> targets)
    {
        if (!obj || !targets) return;

        if (obj.entityClassName != "")
            ExpandKillTarget(targets, obj.entityClassName);

        if (obj.Targets)
        {
            foreach (string target : obj.Targets)
                ExpandKillTarget(targets, target);
        }
    }

    private string GetObjectiveKillLabel(QT_Objective obj)
    {
        if (!obj) return "";
        if (obj.entityClassName != "") return obj.entityClassName;
        if (obj.Targets && obj.Targets.Count() > 0) return obj.Targets[0];
        return "";
    }

    private string BuildVictimClassSignature(EntityAI victim)
    {
        if (!victim) return "";

        string signature = "";
        EntityAI ent = victim;
        int depth = 0;
        while (ent && depth < 8)
        {
            string typeName = ent.GetType();
            if (typeName != "")
            {
                if (signature != "") signature = signature + ">";
                signature = signature + typeName;
            }

            EntityAI parent = EntityAI.Cast(ent.GetHierarchyParent());
            if (!parent || parent == ent) break;
            ent = parent;
            depth++;
        }

        return signature;
    }

    private bool KnownBaseMatches(EntityAI victim, string targetLower)
    {
        if (!victim) return false;

        if (targetLower == "object" || targetLower == "entity" || targetLower == "entityai")
            return true;

        if (targetLower == "zombiebase")
        {
            if (victim.IsInherited(ZombieBase)) return true;
            if (ZombieBase.Cast(victim)) return true;
        }

        if (targetLower == "animalbase")
        {
            if (victim.IsInherited(AnimalBase)) return true;
            if (AnimalBase.Cast(victim)) return true;
        }

        if (targetLower == "itembase")
        {
            if (victim.IsInherited(ItemBase)) return true;
            if (ItemBase.Cast(victim)) return true;
        }

        if (targetLower == "buildingbase")
        {
            if (victim.IsInherited(BuildingBase)) return true;
            if (BuildingBase.Cast(victim)) return true;
        }

        if (targetLower == "house")
        {
            if (victim.IsInherited(House)) return true;
            if (House.Cast(victim)) return true;
        }

        if (targetLower == "carscript")
        {
            if (victim.IsInherited(CarScript)) return true;
            if (CarScript.Cast(victim)) return true;
        }

        if (targetLower == "boatscript")
        {
            if (victim.IsInherited(BoatScript)) return true;
            if (BoatScript.Cast(victim)) return true;
        }

        if (targetLower == "transport")
        {
            if (victim.IsInherited(Transport)) return true;
            if (Transport.Cast(victim)) return true;
        }

        if (targetLower == "playerbase")
        {
            if (victim.IsInherited(PlayerBase)) return true;
            if (PlayerBase.Cast(victim)) return true;
        }

        if (targetLower == "manbase")
        {
            if (victim.IsInherited(ManBase)) return true;
            if (ManBase.Cast(victim)) return true;
        }

        return false;
    }

    private bool ClassNameMatchesTarget(string className, string target)
    {
        if (className == "" || target == "") return false;

        if (className == target) return true;
        if (GetGame().IsKindOf(className, target)) return true;

        string classLower = className;
        classLower.ToLower();
        string targetLower = target;
        targetLower.ToLower();

        if (classLower == targetLower) return true;
        if (classLower.Contains(targetLower)) return true;

        return false;
    }

    private bool DoesVictimInherit(EntityAI victim, string target)
    {
        if (!victim || target == "") return false;

        string targetLower = target;
        targetLower.ToLower();
        if (KnownBaseMatches(victim, targetLower)) return true;

        if (victim.IsKindOf(target)) return true;
        if (GetGame().IsKindOf(victim.GetType(), target)) return true;

        return false;
    }

    private bool KillTargetMatches(EntityAI victim, string target)
    {
        if (!victim) return false;
        target = target.Trim();
        if (target == "") return false;

        string signature = BuildVictimClassSignature(victim);
        string cacheKey = signature + "|" + target;
        if (m_killMatchCache.Contains(cacheKey))
            return m_killMatchCache.Get(cacheKey);

        string targetLower = target;
        targetLower.ToLower();

        bool matched = false;
        if (KnownBaseMatches(victim, targetLower))
        {
            matched = true;
        }
        else if (DoesVictimInherit(victim, target))
        {
            matched = true;
        }
        else
        {
            EntityAI ent = victim;
            int depth = 0;
            while (ent && depth < 8)
            {
                if (ClassNameMatchesTarget(ent.GetType(), target))
                {
                    matched = true;
                    break;
                }

                EntityAI parent = EntityAI.Cast(ent.GetHierarchyParent());
                if (!parent || parent == ent) break;
                ent = parent;
                depth++;
            }
        }

        if (m_killMatchCache.Count() > 2048)
            m_killMatchCache.Clear();
        m_killMatchCache.Insert(cacheKey, matched);
        return matched;
    }

    private bool ObjectiveMatchesKilledEntity(EntityAI victim, QT_Objective obj)
    {
        if (!victim || !obj) return false;
        if (obj.entityClassName == "QT_ReconObjective") return false;

        array<string> targets = new array<string>();
        BuildObjectiveKillTargets(obj, targets);
        if (targets.Count() == 0) return false;

        bool classMatched = false;
        foreach (string target : targets)
        {
            if (KillTargetMatches(victim, target))
            {
                classMatched = true;
                break;
            }
        }

        if (!classMatched) return false;

        // --------------------------------------------------------
        //  Verificação de alvo nomeado específico (specificTargetName).
        //  Quando definido na quest, apenas entidades com esse nome
        //  (GetDisplayName ou m_qtTargetName) contam para o objetivo.
        //  Exemplo de uso no QuestConfig.json:
        //    "specificTargetName": "Coronel Rashid"
        // --------------------------------------------------------
        if (obj.specificTargetName != "")
        {
            string victimDisplay = victim.GetDisplayName();
            string targetNameLower = obj.specificTargetName;
            targetNameLower.ToLower();
            string victimDisplayLower = victimDisplay;
            victimDisplayLower.ToLower();

            // Também tenta GetType para NPCs que usam nome como classname
            string victimType = victim.GetType();
            string victimTypeLower = victimType;
            victimTypeLower.ToLower();

            bool nameMatchedByDisplay = (victimDisplayLower != "" && victimDisplayLower.Contains(targetNameLower));
            bool nameMatchedByType    = (victimTypeLower != "" && victimTypeLower.Contains(targetNameLower));
            bool nameMatched = nameMatchedByDisplay || nameMatchedByType;

            if (!nameMatched) return false;
        }

        return true;
    }

    // ============================================================
    //  Métodos públicos chamados por QT_KillHelper (QT_KillHooks.c).
    //  Estado de attacker por entity é indexado pelo EntityLowId (int).
    //  Armazenamos o UID do jogador (string) em vez do ponteiro ao
    //  PlayerBase para evitar referências inválidas após desconexão.
    // ============================================================

    // Chamado em EEHitBy: registra quem acertou a entidade.
    void QT_StoreAttackerUID(EntityAI victim, string attackerUID)
    {
        if (!victim || attackerUID == "") return;
        int id = victim.GetID();
        m_qt_lastAttackerUID.Set(id, attackerUID);
        m_qt_lastHitTime.Set(id, GetGame().GetTime() / 1000);
    }

    // Chamado em EEKilled: devolve o PlayerBase do último atacante
    // dentro da janela de bleed-out, se ainda estiver online.
    PlayerBase QT_ResolveStoredAttacker(EntityAI victim, int now, int expirySeconds)
    {
        if (!victim) return null;
        int id = victim.GetID();
        if (!m_qt_lastAttackerUID.Contains(id)) return null;
        if (!m_qt_lastHitTime.Contains(id)) return null;
        int hitTime = m_qt_lastHitTime.Get(id);
        if ((now - hitTime) > expirySeconds) return null;
        string uid = m_qt_lastAttackerUID.Get(id);
        return FindOnlinePlayer(uid);
    }

    bool QT_IsRecentlyKillCredited(EntityAI victim, int now, int dedupWindow)
    {
        if (!victim) return false;
        int id = victim.GetID();
        if (!m_qt_creditedKillTime.Contains(id)) return false;
        return (now - m_qt_creditedKillTime.Get(id)) < dedupWindow;
    }

    void QT_MarkKillCredited(EntityAI victim, int now)
    {
        if (!victim) return;
        m_qt_creditedKillTime.Set(victim.GetID(), now);
    }

    void OnEntityKilled(EntityAI victim, PlayerBase killerPlayer)
    {
        if (!GetGame().IsServer()) return;
        if (!m_config) return;
        if (!victim || !killerPlayer || !killerPlayer.GetIdentity()) return;
        string uid = killerPlayer.GetIdentity().GetId();
        string victimClass = victim.GetType();
        auto playerMap = GetOrCreatePlayerMap(uid);
        bool anyChange = false;
        bool killDebugEnabled = IsKillDebugEnabled();
        if (killDebugEnabled)
        {
            KillDebug("Entity killed detected", killerPlayer);
            KillDebug("Victim classname: " + victimClass, killerPlayer);
            KillDebug("Victim inherited from ZombieBase: " + BoolText(DoesVictimInherit(victim, "ZombieBase")), killerPlayer);
            KillDebug("Victim inherited from AnimalBase: " + BoolText(DoesVictimInherit(victim, "AnimalBase")), killerPlayer);
            KillDebug("Victim inherited from ItemBase: " + BoolText(DoesVictimInherit(victim, "ItemBase")), killerPlayer);
            KillDebug("Victim inherited from BuildingBase: " + BoolText(DoesVictimInherit(victim, "BuildingBase")), killerPlayer);
            KillDebug("Victim inherited from CarScript: " + BoolText(DoesVictimInherit(victim, "CarScript")), killerPlayer);
            KillDebug("Victim inherited from BoatScript: " + BoolText(DoesVictimInherit(victim, "BoatScript")), killerPlayer);
            KillDebug("Killer: " + killerPlayer.GetIdentity().GetName(), killerPlayer);
        }

        foreach (string questId, ref QT_PlayerQuestState qs : playerMap)
        {
            if (!qs || qs.state != QT_QuestState.ACTIVE) continue;
            QT_QuestDef def = GetQuestDef(questId);
            if (!def || def.type != QT_QuestType.KILL) continue;

            EnsureObjectiveProgress(qs, def);
            bool questChanged = false;

            foreach (int idx, QT_Objective obj : def.objectives)
            {
                if (!obj) continue;
                if (idx >= qs.objectiveProgress.Count()) continue;

                bool matched = ObjectiveMatchesKilledEntity(victim, obj);
                if (killDebugEnabled)
                    KillDebug("Quest '" + def.id + "' objective '" + obj.entityClassName + "' matched: " + BoolText(matched), killerPlayer);
                if (!matched) continue;

                int cur = qs.objectiveProgress[idx];
                if (cur >= obj.requiredAmount) continue;
                qs.objectiveProgress.Set(idx, cur + 1);
                anyChange = true;
                questChanged = true;

                if (killDebugEnabled)
                    KillDebug("Quest progress updated: " + def.id + " " + (cur + 1).ToString() + "/" + obj.requiredAmount.ToString(), killerPlayer);

                if (m_config.Settings.notifyOnKillProgress)
                {
                    string toastMsg = "[" + def.title + "]";
                    toastMsg = toastMsg + " ";
                    toastMsg = toastMsg + GetObjectiveKillLabel(obj);
                    toastMsg = toastMsg + " ";
                    toastMsg = toastMsg + (cur + 1).ToString();
                    toastMsg = toastMsg + "/";
                    toastMsg = toastMsg + obj.requiredAmount.ToString();
                    QT_RPCManager.SendToast(killerPlayer, toastMsg, QT_ToastType.PROGRESS);
                }
            }

            if (questChanged && CheckKillQuestComplete(qs, def))
            {
                qs.state = QT_QuestState.COMPLETED;
                QT_RPCManager.SendToast(killerPlayer, "#QuestTrader_QUEST_READY_TURN_IN: " + def.title, QT_ToastType.COMPLETE);
                if (killDebugEnabled)
                    KillDebug("Quest completed: " + def.id, killerPlayer);
            }
        }

        if (anyChange)
        {
            MarkPlayerDirty(uid);
            QT_RPCManager.SendHUDUpdate(killerPlayer);
        }
    }

    void AdminResetQuest(string uid, string questInput)
    {
        auto playerMap = GetOrCreatePlayerMap(uid);
        questInput = questInput.Trim();
        if (questInput == "")
        {
            playerMap.Clear();
            QT_QuestHistory.GetInstance().DeleteHistory(uid);
        }
        else
        {
            QT_QuestDef def = ResolveQuestForAdmin(uid, questInput);
            if (!def)
            {
                QT_Logger.GetInstance().Warn("ADMIN", "AdminResetQuest: could not resolve quest from input: '" + questInput + "' for " + uid);
                return;
            }
            string questId = def.id;
            if (playerMap.Contains(questId)) playerMap.Remove(questId);
            QT_QuestHistory.GetInstance().DeleteQuestHistory(uid, questId);
        }
        SavePlayerImmediate(uid, "admin reset");

        PlayerBase player = FindOnlinePlayer(uid);
        if (player)
            QT_RPCManager.SendHUDUpdate(player);
    }

    // Store last known nearby trader per player for F-key interaction
    private ref map<string, string> m_nearbyTrader = new map<string, string>();

    void SetNearbyTraderId(string uid, string traderId)
    {
        m_nearbyTrader.Set(uid, traderId);
    }

    string GetNearbyTraderId(string uid)
    {
        if (m_nearbyTrader.Contains(uid)) return m_nearbyTrader.Get(uid);
        return "";
    }

    void AdminWipePlayer(string uid)
    {
        if (m_playerStates.Contains(uid)) m_playerStates.Remove(uid);
        QT_PersistenceManager.DeletePlayer(uid);
        QT_QuestHistory.GetInstance().DeleteHistory(uid);
        if (m_DirtyPlayers.Contains(uid)) m_DirtyPlayers.Set(uid, false);
        RemoveFromSaveQueue(uid);

        PlayerBase player = FindOnlinePlayer(uid);
        if (player)
        {
            m_playerStates.Insert(uid, new map<string, ref QT_PlayerQuestState>());
            QT_RPCManager.SendHUDUpdate(player);
            QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_ADMIN_WIPED_PLAYER", QT_ToastType.WARNING);
        }
    }

    bool AdminCompleteQuest(string uid, string questInput)
    {
        auto playerMap = GetOrCreatePlayerMap(uid);
        QT_QuestDef def = ResolveQuestForAdmin(uid, questInput);
        if (!def)
        {
            QT_Logger.GetInstance().Warn("ADMIN", "AdminCompleteQuest: could not resolve quest from input: '" + questInput + "' for " + uid);
            return false;
        }

        string questId = def.id;
        QT_PlayerQuestState qs;
        if (playerMap.Contains(questId))
            qs = playerMap.Get(questId);
        else
        {
            qs = new QT_PlayerQuestState();
            qs.questId = questId;
            qs.state = QT_QuestState.ACTIVE;
            playerMap.Insert(questId, qs);
        }

        if (!qs) return false;
        if (qs.state == QT_QuestState.TURNED_IN || qs.state == QT_QuestState.COOLDOWN)
        {
            QT_Logger.GetInstance().Warn("ADMIN", "AdminCompleteQuest: quest already finished: " + questId + " for " + uid);
            return false;
        }

        qs.questId = questId;

        for (int i = 0; i < def.objectives.Count(); i++)
        {
            while (qs.objectiveProgress.Count() <= i)
                qs.objectiveProgress.Insert(0);
            qs.objectiveProgress.Set(i, def.objectives[i].requiredAmount);
        }

        PlayerBase player = FindOnlinePlayer(uid);
        if (player)
        {
            GiveRewards(player, def);
            RecordCompletion(player, def);
            if (def.rewardMessage != "")
                QT_RPCManager.SendToast(player, def.rewardMessage, QT_ToastType.REWARD);
            SendQuestCompleteNotification(player, def);
            QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_ADMIN_COMPLETED_FOR_YOU", QT_ToastType.INFO);
        }
        else
        {
            QT_QuestHistory.GetInstance().RecordCompletion(uid, uid, def, GetGame().GetTime() / 1000);
        }

        CleanupQuestItem(uid, questId);

        if (def.repeatable) qs.state = QT_QuestState.COOLDOWN;
        else                qs.state = QT_QuestState.TURNED_IN;
        qs.completedTimestamp = GetGame().GetTime() / 1000;

        SavePlayerImmediate(uid, "admin complete");
        if (player) QT_RPCManager.SendHUDUpdate(player);
        return true;
    }

    private PlayerBase FindOnlinePlayer(string uid)
    {
        return FindOnlinePlayerObject(uid, uid);
    }

    private PlayerBase FindOnlinePlayerObject(string uid, string steamId = "")
    {
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase pb = PlayerBase.Cast(man);
            if (!pb || !pb.GetIdentity()) continue;
            if (pb.GetIdentity().GetId() == uid)
                return pb;
            if (steamId != "" && pb.GetIdentity().GetPlainId() == steamId)
                return pb;
            if (pb.GetIdentity().GetPlainId() == uid)
                return pb;
        }
        return null;
    }

    private void RecordCompletion(PlayerBase player, QT_QuestDef def)
    {
        string uid  = player.GetIdentity().GetId();
        string name = player.GetIdentity().GetName();
        QT_QuestHistory.GetInstance().RecordCompletion(uid, name, def, GetGame().GetTime() / 1000);
    }

    private bool CheckKillQuestComplete(QT_PlayerQuestState qs, QT_QuestDef def)
    {
        foreach (int i, QT_Objective obj : def.objectives)
        {
            if (i >= qs.objectiveProgress.Count()) return false;
            if (qs.objectiveProgress[i] < obj.requiredAmount) return false;
        }
        return true;
    }

    int CountItemsInInventory(PlayerBase player, string className)
    {
        int count = 0;
        array<EntityAI> items = new array<EntityAI>();
        player.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
        foreach (EntityAI item : items)
        {
            if (item.GetType() != className && !item.IsKindOf(className)) continue;
            ItemBase ib = ItemBase.Cast(item);
            if (!ib) { count += 1; continue; }

            // Ammo piles — sum actual round count
            Ammunition_Base ammo = Ammunition_Base.Cast(ib);
            if (ammo)
            {
                count += ammo.GetAmmoCount();
                continue;
            }

            // Items where GetQuantity() represents individual units (tablets, pills, etc.)
            // rather than a fill level — count by quantity, not by container
            if (IsQuantityCountedItem(className))
            {
                count += (int)ib.GetQuantity();
                continue;
            }

            // Everything else (food, water, containers, tools, weapons) —
            // each item counts as 1 regardless of fill level
            count += 1;
        }
        return count;
    }

    // Returns true for items where GetQuantity() represents discrete units
    // (tablet packs, pill bottles, etc.) rather than a liquid/food fill level.
    // Configurable via QuestItemSettings.json in the server profile directory.
    private bool IsQuantityCountedItem(string className)
    {
        if (!m_itemSettings) return false;
        foreach (string entry : m_itemSettings.QuantityCountedItems)
        {
            if (entry == className) return true;
        }
        return false;
    }

    private void RemoveItemsFromInventory(PlayerBase player, string className, int amount)
    {
        int remaining = amount;
        array<EntityAI> items = new array<EntityAI>();
        player.GetInventory().EnumerateInventory(InventoryTraversalType.INORDER, items);
        foreach (EntityAI item : items)
        {
            if (remaining <= 0) break;
            if (item.GetType() != className && !item.IsKindOf(className)) continue;
            ItemBase ib = ItemBase.Cast(item);
            if (!ib) { remaining--; GetGame().ObjectDelete(item); continue; }

            // Ammo piles — remove by round count
            Ammunition_Base ammo = Ammunition_Base.Cast(ib);
            if (ammo)
            {
                int ammoQty = ammo.GetAmmoCount();
                if (ammoQty <= remaining) { remaining -= ammoQty; GetGame().ObjectDelete(item); }
                else                      { ammo.ServerSetAmmoCount(ammoQty - remaining); remaining = 0; }
                continue;
            }

            // Tablet/pill items — remove by unit quantity
            if (IsQuantityCountedItem(className))
            {
                int tabQty = (int)ib.GetQuantity();
                if (tabQty <= remaining) { remaining -= tabQty; GetGame().ObjectDelete(item); }
                else                     { ib.SetQuantity(tabQty - remaining); remaining = 0; }
                continue;
            }

            // Everything else — each item counts as 1, delete the whole object
            remaining--;
            GetGame().ObjectDelete(item);
        }
    }

    private void SendQuestCompleteNotification(PlayerBase player, QT_QuestDef def)
    {
        string rewardStr = "";
        foreach (QT_Reward rew : def.rewards)
        {
            if (rewardStr != "") rewardStr = rewardStr + ", ";
            rewardStr = rewardStr + rew.amount.ToString() + "x " + QT_RPCManager.GetDisplayName(rew.itemClassName);
        }
        string line1 = "#QuestTrader_CHAT_QUEST_COMPLETE: " + def.title;
        string line2 = "#QuestTrader_CHAT_REWARDS: " + rewardStr;
        QT_RPCManager.SendChatLine(player, line1);
        QT_RPCManager.SendChatLine(player, line2);
        if (ShouldPlaySuccessSound())
            QT_RPCManager.SendSuccessSound(player);
    }

    private bool ShouldPlaySuccessSound()
    {
        if (!m_config || !m_config.Settings) return true;
        return m_config.Settings.ActivateSucessSound;
    }

    private bool IsStackableReward(string className)
    {
        if (!m_itemSettings) return false;
        foreach (string entry : m_itemSettings.StackableRewardItems)
        {
            if (entry == className) return true;
        }
        return false;
    }

    private string BuildRewardString(QT_QuestDef def)
    {
        string rewards = "";
        foreach (QT_Reward r : def.rewards)
        {
            if (rewards != "") rewards += ", ";
            rewards += r.amount.ToString() + "x " + r.itemClassName;
        }
        if (rewards != "") rewards = " [" + rewards + "]";
        return rewards;
    }

    private void GiveRewards(PlayerBase player, QT_QuestDef def)
    {
        foreach (QT_Reward reward : def.rewards)
        {
            // Stackable items (currency etc.) — spawn once and set quantity
            if (IsStackableReward(reward.itemClassName))
            {
                EntityAI item = player.GetInventory().CreateInInventory(reward.itemClassName);
                if (!item)
                {
                    vector dropPos = player.GetPosition();
                    dropPos[1] = dropPos[1] + 0.3;
                    item = EntityAI.Cast(GetGame().CreateObject(reward.itemClassName, dropPos));
                }
                if (item)
                {
                    ItemBase ib = ItemBase.Cast(item);
                    if (ib) ib.SetQuantity(reward.amount);
                }
                continue;
            }

            // Everything else — spawn individually
            for (int n = 0; n < reward.amount; n++)
            {
                EntityAI singleItem = player.GetInventory().CreateInInventory(reward.itemClassName);
                if (!singleItem)
                {
                    vector pos = player.GetPosition();
                    pos[1] = pos[1] + 0.3;
                    GetGame().CreateObject(reward.itemClassName, pos);
                }
            }
        }
    }

    static void QT_Notify(PlayerBase player, string msg)
    {
        if (!player || !player.GetIdentity()) return;
        QT_RPCManager.SendChatLine(player, "#QuestTrader_CHAT_TRADER " + msg);
    }
}
