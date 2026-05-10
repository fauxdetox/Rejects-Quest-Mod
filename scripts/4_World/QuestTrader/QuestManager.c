// ============================================================
//  QuestTrader | QuestManager.c  (v1.5)
//  QT_ToastType now defined in QuestData.c so both
//  4_World and 5_Mission can use it.
// ============================================================

class QT_QuestManager
{
    private static ref QT_QuestManager s_instance;
    private ref QT_Config m_config;
    private ref map<string, ref map<string, ref QT_PlayerQuestState>> m_playerStates;
    private ref QT_ItemSettings m_itemSettings;
    // uid+questId -> spawned world object (for cleanup on cancel/complete)
    private ref map<string, Object> m_spawnedQuestItems;
    // uid+questId -> array of spawned entities (animals/zombies) for reset on reaccept
    private ref map<string, ref array<Object>> m_spawnedQuestEntities;

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
        m_spawnedQuestItems    = new map<string, Object>();
        m_spawnedQuestEntities = new map<string, ref array<Object>>();
        m_config            = QT_ConfigLoader.LoadConfig();
        m_itemSettings      = QT_ItemSettingsLoader.LoadItemSettings();
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
        SyncLoadedPlayerStatesWithConfig();

        QT_Logger.GetInstance().Info("ADMIN", "Config reloaded. " + m_config.TraderNPCPositions.Count() + " traders, " + m_config.Quests.Count() + " quests.");
        return true;
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
        // Guard: don't reload if already in memory (OnConnect can fire twice)
        if (m_playerStates.Contains(uid))
        {
            QT_Logger.GetInstance().Info("CONNECT", "Player reconnected (data already in memory).", uid);
            return;
        }

        ref map<string, ref QT_PlayerQuestState> loaded = QT_PersistenceManager.LoadPlayer(uid);
        if (loaded)
            m_playerStates.Insert(uid, loaded);
        else
            m_playerStates.Insert(uid, new map<string, ref QT_PlayerQuestState>());
        QT_Logger.GetInstance().Info("CONNECT", "Player connected.", uid);
    }

    void OnPlayerDisconnected(string uid)
    {
        if (m_playerStates.Contains(uid))
        {
            // Save first, then remove from memory
            auto questMap = m_playerStates.Get(uid);
            if (questMap)
            {
                Print("[QuestTrader] Saving on disconnect for: " + uid + " (" + questMap.Count() + " quests)");
                QT_PersistenceManager.SavePlayer(uid, questMap);
            }
            m_playerStates.Remove(uid);
        }
        QT_Logger.GetInstance().Info("CONNECT", "Player disconnected.", uid);
    }

    void SaveAll()
    {
        foreach (string uid, map<string, ref QT_PlayerQuestState> questMap : m_playerStates)
            QT_PersistenceManager.SavePlayer(uid, questMap);
        QT_Logger.GetInstance().Info("PERSIST", "All player data saved.");
    }

    ref map<string, ref QT_PlayerQuestState> GetOrCreatePlayerMap(string uid)
    {
        if (!m_playerStates.Contains(uid))
        {
            // Try loading from disk first before creating a blank map
            ref map<string, ref QT_PlayerQuestState> loaded = QT_PersistenceManager.LoadPlayer(uid);
            if (loaded)
            {
                Print("[QuestTrader] GetOrCreatePlayerMap - loaded from disk for: " + uid);
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

    QT_QuestDef GetQuestDef(string questId)
    {
        if (!m_config) return null;
        foreach (QT_QuestDef def : m_config.Quests)
            if (def.id == questId) return def;
        return null;
    }

    // Admin helper: resolve a quest by ID, title, or (if blank) the player's first active quest.
    // Allows admins to type either the internal quest_ID or the human-readable quest title.
    QT_QuestDef ResolveQuestForAdmin(string uid, string input)
    {
        if (!m_config) return null;

        // Blank input -> use the player's first active quest
        string trimmed = input.Trim();
        if (trimmed == "")
        {
            array<ref QT_QuestDef> active = GetActiveQuestsForPlayer(uid);
            if (active.Count() > 0) return active[0];
            return null;
        }

        // Exact ID match
        foreach (QT_QuestDef def : m_config.Quests)
            if (def.id == trimmed) return def;

        // Case-insensitive title match
        string inputLower = trimmed;
        inputLower.ToLower();
        foreach (QT_QuestDef defT : m_config.Quests)
        {
            string t = defT.title;
            t.ToLower();
            if (t == inputLower) return defT;
        }

        // Partial title match (contains)
        foreach (QT_QuestDef defP : m_config.Quests)
        {
            string tp = defP.title;
            tp.ToLower();
            if (tp.Contains(inputLower)) return defP;
        }

        return null;
    }

    array<ref QT_QuestDef> GetQuestsForTrader(string traderId, string playerUID)
    {
        array<ref QT_QuestDef> result = new array<ref QT_QuestDef>();
        if (!m_config) return result;
        foreach (QT_QuestDef def : m_config.Quests)
        {
            QT_QuestState state = GetEffectiveState(playerUID, def.id);

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
            if (GetEffectiveState(uid, preReqId) != QT_QuestState.TURNED_IN)
                return false;
        }
        return true;
    }

    array<ref QT_QuestDef> GetActiveQuestsForPlayer(string uid)
    {
        array<ref QT_QuestDef> result = new array<ref QT_QuestDef>();
        if (!m_config) return result;
        foreach (QT_QuestDef def : m_config.Quests)
        {
            QT_QuestState st = GetEffectiveState(uid, def.id);
            if (st == QT_QuestState.ACTIVE || st == QT_QuestState.COMPLETED)
                result.Insert(def);
        }
        return result;
    }

    QT_QuestState GetDisplayState(PlayerBase player, QT_QuestDef def)
    {
        if (!player || !player.GetIdentity() || !def)
            return QT_QuestState.AVAILABLE;

        QT_QuestState state = GetEffectiveState(player.GetIdentity().GetId(), def.id);
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
        if (qs.state == QT_QuestState.COOLDOWN)
        {
            QT_QuestDef def = GetQuestDef(questId);
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
            QT_QuestDef kDef = GetQuestDef(questId);
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
            if (GetEffectiveState(uid, preReqId) != QT_QuestState.TURNED_IN)
            {
                QT_QuestDef pre = GetQuestDef(preReqId);
                string preName = preReqId;
                if (pre) preName = pre.title;
                QT_RPCManager.SendToast(player, "Complete '" + preName + "' first!", QT_ToastType.WARNING);
                string prereqMsg = "[Quest] Complete '" + preName + "' before accepting this quest.";
                GetGame().RPCSingleParam(player, ERPCs.RPC_USER_ACTION_MESSAGE,
                    new Param1<string>(prereqMsg), true, player.GetIdentity());
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

        QT_PersistenceManager.SavePlayer(uid, GetOrCreatePlayerMap(uid));
        QT_RPCManager.SendToast(player, "#QuestTrader_TOAST_QUEST_ACCEPTED " + def.title, QT_ToastType.ACCEPT);
        QT_RPCManager.SendQuestInfo(player, def);
        QT_RPCManager.SendHUDUpdate(player);
        QT_Logger.GetInstance().Info("QUEST", "Accepted: " + def.title, uid, player.GetIdentity().GetName());

        // Spawn quest entities nearby for kill quests
        if (def.type == QT_QuestType.KILL)
            SpawnQuestEntities(player, def);

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
                    string key = uid + "_" + def.id + "_" + si.itemClass;
                    m_spawnedQuestItems.Insert(key, obj);
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
        // Infected and anything already prefixed pass through directly
        return entityClassName;
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

            // Pick a spawn centre 150-200m away in a random direction
            float angle = Math.RandomFloat(0, Math.PI2);
            float dist  = Math.RandomFloat(150, 200);
            vector spawnCenter;
            spawnCenter[0] = playerPos[0] + Math.Sin(angle) * dist;
            spawnCenter[2] = playerPos[2] + Math.Cos(angle) * dist;
            spawnCenter[1] = GetGame().SurfaceY(spawnCenter[0], spawnCenter[2]);

            for (int i = 0; i < obj.requiredAmount; i++)
            {
                // Try up to 5 positions, pick one that has open sky above it
                vector spawnPos = spawnCenter;
                for (int attempt = 0; attempt < 5; attempt++)
                {
                    float scatter = Math.RandomFloat(0, Math.PI2);
                    float scatterDist = Math.RandomFloat(3, 20);
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

                Object spawned = GetGame().CreateObject(spawnClass, spawnPos, false, true);
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
        QT_PersistenceManager.SavePlayer(uid, playerMap);

        if (questId == "quest_201")
        {
            SpawnHelicopterCrash("quest_201");
            QT_RPCManager.SendToast(player, "Recon Complete! Get back to Sister Emma!", QT_ToastType.ACCEPT);
        }
        else
        {
            SpawnHelicopterCrash("quest_179");
            QT_RPCManager.SendToast(player, "Recon Complete! Let's get back to Fisher!", QT_ToastType.ACCEPT);
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
            QT_RPCManager.SendToast(player, "No active quest to cancel.", QT_ToastType.WARNING);
            return false;
        }

        // Reset state and all progress
        qs.state = QT_QuestState.AVAILABLE;
        qs.completedTimestamp = 0;
        for (int ci = 0; ci < qs.objectiveProgress.Count(); ci++)
            qs.objectiveProgress.Set(ci, 0);

        QT_PersistenceManager.SavePlayer(uid, GetOrCreatePlayerMap(uid));
        QT_RPCManager.SendToast(player, "Quest cancelled.", QT_ToastType.WARNING);
        QT_RPCManager.SendHUDUpdate(player);
        QT_Logger.GetInstance().Info("QUEST", "Cancelled: " + questId, uid, name);
        CleanupQuestItem(uid, questId);

        // Remove delivery item from inventory if this was a deliver quest
        QT_QuestDef def = GetQuestDef(questId);
        if (def && def.type == QT_QuestType.DELIVER)
            RemoveDeliveryItem(player, def);

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
            QT_RPCManager.SendToast(player, "You have not accepted this quest.", QT_ToastType.WARNING);
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

        QT_PersistenceManager.SavePlayer(uid, GetOrCreatePlayerMap(uid));
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
            QT_RPCManager.SendToast(player, "You have not accepted this quest.", QT_ToastType.WARNING);
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

        QT_PersistenceManager.SavePlayer(uid, GetOrCreatePlayerMap(uid));
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
            string prog = "Incomplete:";
            foreach (int i, QT_Objective obj : def.objectives)
            {
                int cur = 0;
                if (i < qs.objectiveProgress.Count()) cur = qs.objectiveProgress[i];
                prog = prog + " " + obj.entityClassName + " " + cur + "/" + obj.requiredAmount;
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

        QT_PersistenceManager.SavePlayer(uid, GetOrCreatePlayerMap(uid));
        if (def.rewardMessage != "")
            QT_RPCManager.SendToast(player, def.rewardMessage, QT_ToastType.REWARD);
        SendQuestCompleteNotification(player, def);
        QT_RPCManager.SendHUDUpdate(player);
        QT_Logger.GetInstance().Info("QUEST", "Completed (kill): " + def.title, uid, name);
        return true;
    }

    void OnEntityKilled(EntityAI victim, PlayerBase killerPlayer)
    {
        if (!GetGame().IsServer()) return;
        if (!m_config) return;
        Print("[QuestTrader] OnEntityKilled: " + victim.GetType() + " killed by " + killerPlayer.GetIdentity().GetName());
        if (!killerPlayer) return;

        string uid = killerPlayer.GetIdentity().GetId();
        string victimClass = victim.GetType();
        auto playerMap = GetOrCreatePlayerMap(uid);
        bool anyChange = false;

        foreach (string questId, ref QT_PlayerQuestState qs : playerMap)
        {
            if (qs.state != QT_QuestState.ACTIVE) continue;
            QT_QuestDef def = GetQuestDef(questId);
            if (!def || def.type != QT_QuestType.KILL) continue;

            foreach (int idx, QT_Objective obj : def.objectives)
            {
                if (obj.entityClassName == "") continue;
                if (obj.entityClassName == "QT_ReconObjective") continue;

                // Check if this kill matches the objective
                bool matched = false;
                if (obj.entityClassName == "Deer" && m_itemSettings)
                {
                    // Any deer variant counts
                    foreach (string alias : m_itemSettings.DeerKillAliases)
                    {
                        if (victimClass.Contains(alias)) { matched = true; break; }
                    }
                }
                else
                {
                    matched = victimClass.Contains(obj.entityClassName);
                }

                if (!matched) continue;
                int cur = qs.objectiveProgress[idx];
                if (cur >= obj.requiredAmount) continue;
                qs.objectiveProgress.Set(idx, cur + 1);
                anyChange = true;
                if (m_config.Settings.notifyOnKillProgress)
                {
                    QT_RPCManager.SendToast(killerPlayer, "[" + def.title + "] " + obj.entityClassName + " " + (cur + 1) + "/" + obj.requiredAmount, QT_ToastType.PROGRESS);
                    // Also send to chat so it's visible
                    string progressMsg = "[Quest Progress] " + def.title + ": Kills " + (cur + 1) + "/" + obj.requiredAmount;
                    GetGame().RPCSingleParam(killerPlayer, ERPCs.RPC_USER_ACTION_MESSAGE,
                        new Param1<string>(progressMsg), true, killerPlayer.GetIdentity());
                }
            }

            if (anyChange && CheckKillQuestComplete(qs, def))
            {
                qs.state = QT_QuestState.COMPLETED;
                QT_RPCManager.SendToast(killerPlayer, "Quest complete! Return to trader: " + def.title, QT_ToastType.REWARD);
                // Also send chat message so it's hard to miss
                string completeMsg = "[Quest Complete] " + def.title + " - Return to the Quest Trader to claim your reward!";
                GetGame().RPCSingleParam(killerPlayer, ERPCs.RPC_USER_ACTION_MESSAGE,
                    new Param1<string>(completeMsg), true, killerPlayer.GetIdentity());
            }
        }

        if (anyChange)
        {
            QT_PersistenceManager.SavePlayer(uid, playerMap);
            QT_RPCManager.SendHUDUpdate(killerPlayer);
        }
    }

    void AdminResetQuest(string uid, string input)
    {
        QT_QuestDef def = ResolveQuestForAdmin(uid, input);
        if (!def)
        {
            QT_Logger.GetInstance().Warn("ADMIN", "AdminResetQuest: could not resolve quest from input: '" + input + "' for " + uid);
            return;
        }
        auto playerMap = GetOrCreatePlayerMap(uid);
        if (playerMap.Contains(def.id)) playerMap.Remove(def.id);
        QT_PersistenceManager.SavePlayer(uid, playerMap);
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

        // Push a cleared HUD update to the wiped player if they are online
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase pb = PlayerBase.Cast(man);
            if (!pb || !pb.GetIdentity()) continue;
            if (pb.GetIdentity().GetId() == uid)
            {
                QT_RPCManager.SendHUDUpdate(pb);
                QT_RPCManager.SendToast(pb, "Your quests have been reset by an admin.", QT_ToastType.WARNING);
                break;
            }
        }
    }

    // Admin force-completes a player's active quest, giving rewards
    void AdminCompleteQuest(string uid, string input)
    {
        auto playerMap = GetOrCreatePlayerMap(uid);
        QT_QuestDef def = ResolveQuestForAdmin(uid, input);
        if (!def)
        {
            QT_Logger.GetInstance().Warn("ADMIN", "AdminCompleteQuest: could not resolve quest from input: '" + input + "' for " + uid);
            return;
        }
        string questId = def.id;

        auto qs = GetPlayerQuestState(uid, questId);
        if (qs.state != QT_QuestState.ACTIVE && qs.state != QT_QuestState.COMPLETED) return;

        // Mark all kill objectives as complete so state resolves correctly
        for (int i = 0; i < def.objectives.Count(); i++)
        {
            while (qs.objectiveProgress.Count() <= i)
                qs.objectiveProgress.Insert(0);
            qs.objectiveProgress.Set(i, def.objectives[i].requiredAmount);
        }

        // Find the online player to give rewards; skip reward if offline
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);
        PlayerBase target = null;
        foreach (Man man : players)
        {
            PlayerBase pb = PlayerBase.Cast(man);
            if (!pb || !pb.GetIdentity()) continue;
            if (pb.GetIdentity().GetId() == uid) { target = pb; break; }
        }

        if (target)
        {
            GiveRewards(target, def);
            RecordCompletion(target, def);
            if (def.rewardMessage != "")
                QT_RPCManager.SendToast(target, def.rewardMessage, QT_ToastType.REWARD);
            QT_RPCManager.SendToast(target, "A quest has been completed for you by an admin.", QT_ToastType.INFO);
        }
        else
        {
            // Player is offline — record completion without giving rewards
            string emptyName = uid;
            QT_QuestHistory.GetInstance().RecordCompletion(uid, emptyName, def, GetGame().GetTime() / 1000);
        }

        CleanupQuestItem(uid, questId);

        if (def.repeatable) qs.state = QT_QuestState.COOLDOWN;
        else                qs.state = QT_QuestState.TURNED_IN;
        qs.completedTimestamp = GetGame().GetTime() / 1000;

        QT_PersistenceManager.SavePlayer(uid, playerMap);

        if (target) QT_RPCManager.SendHUDUpdate(target);
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
        string line1 = "*** QUEST COMPLETE: " + def.title + " ***";
        string line2 = "Rewards: " + rewardStr;
        GetGame().RPCSingleParam(player, ERPCs.RPC_USER_ACTION_MESSAGE,
            new Param1<string>(line1), true, player.GetIdentity());
        GetGame().RPCSingleParam(player, ERPCs.RPC_USER_ACTION_MESSAGE,
            new Param1<string>(line2), true, player.GetIdentity());
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
        GetGame().RPCSingleParam(player, ERPCs.RPC_USER_ACTION_MESSAGE, new Param1<string>("[Trader] " + msg), true, player.GetIdentity());
    }
}
