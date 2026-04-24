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

    static QT_QuestManager GetInstance()
    {
        if (!s_instance) s_instance = new QT_QuestManager();
        return s_instance;
    }

    void QT_QuestManager()
    {
        // Register RPC dispatcher so 3_Game hook can call into 4_World
        QT_RPCDispatcherBase.s_instance = new QT_RPCDispatcher();
        m_playerStates = new map<string, ref map<string, ref QT_PlayerQuestState>>();
        m_config = QT_ConfigLoader.LoadConfig();
        m_itemSettings = QT_ItemSettingsLoader.LoadItemSettings();
        int qc = 0;
        if (m_config) qc = m_config.Quests.Count();
        QT_Logger.GetInstance().Info("SYSTEM", "QuestManager initialised. " + qc + " quests loaded.");
    }

    QT_Config GetConfig() { return m_config; }

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

    array<ref QT_QuestDef> GetQuestsForTrader(string traderId, string playerUID)
    {
        array<ref QT_QuestDef> result = new array<ref QT_QuestDef>();
        if (!m_config) return result;
        foreach (QT_QuestDef def : m_config.Quests)
        {
            // Normal quests: show at their origin trader
            if (def.traderId == traderId)
            {
                result.Insert(def);
                continue;
            }
            // Delivery quests: also show at the delivery trader when active or completed
            if (def.type == QT_QuestType.DELIVER && def.deliveryTraderId == traderId)
            {
                QT_QuestState st = GetEffectiveState(playerUID, def.id);
                if (st == QT_QuestState.ACTIVE || st == QT_QuestState.COMPLETED)
                    result.Insert(def);
            }
        }
        return result;
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

        // Only allow one active quest at a time across all traders
        auto playerMap = GetOrCreatePlayerMap(uid);
        foreach (string existingId, QT_PlayerQuestState existingQs : playerMap)
        {
            if (existingId == questId) continue; // don't check the quest being accepted
            QT_QuestState effectiveState = GetEffectiveState(uid, existingId);
            if (effectiveState != QT_QuestState.ACTIVE && effectiveState != QT_QuestState.COMPLETED) continue;
            QT_RPCManager.SendToast(player, "Finish your current quest before accepting another!", QT_ToastType.WARNING);
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
            QT_RPCManager.SendToast(player, "Quest not available.", QT_ToastType.WARNING);
            return false;
        }

        auto qs = GetPlayerQuestState(uid, questId);
        qs.state = QT_QuestState.ACTIVE;

        // Give delivery item immediately on accept
        if (def.type == QT_QuestType.DELIVER)
            GiveDeliveryItem(player, def);

        QT_PersistenceManager.SavePlayer(uid, GetOrCreatePlayerMap(uid));
        QT_RPCManager.SendToast(player, "Quest accepted: " + def.title, QT_ToastType.ACCEPT);
        QT_RPCManager.SendQuestInfo(player, def);
        QT_RPCManager.SendHUDUpdate(player);
        QT_Logger.GetInstance().Info("QUEST", "Accepted: " + def.title, uid, player.GetIdentity().GetName());
        return true;
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
            QT_RPCManager.SendToast(player, "You lost the delivery item!", QT_ToastType.WARNING);
            return false;
        }

        RemoveDeliveryItem(player, def);
        GiveRewards(player, def);
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
                QT_RPCManager.SendToast(player, "Still need " + need + "x " + obj.itemClassName, QT_ToastType.WARNING);
                return false;
            }
        }

        foreach (QT_Objective obj2 : def.objectives)
            RemoveItemsFromInventory(player, obj2.itemClassName, obj2.requiredAmount);

        GiveRewards(player, def);
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

    void AdminResetQuest(string uid, string questId)
    {
        auto playerMap = GetOrCreatePlayerMap(uid);
        if (playerMap.Contains(questId)) playerMap.Remove(questId);
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
        // Build reward summary
        string rewardStr = "";
        foreach (QT_Reward rew : def.rewards)
        {
            if (rewardStr != "") rewardStr = rewardStr + ", ";
            rewardStr = rewardStr + rew.amount + "x " + rew.itemClassName;
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
