// ============================================================
//  QuestTrader | QT_TraderSpawner.c  (v2.1)
//
//  Custom AI classes don't spawn via CreateObject.
//  Instead: spawn vanilla SurvivorBase models and track which
//  entity is a trader using a static map (entityID -> traderId).
//  ActionInteractTrader checks this map to identify traders.
//
//  Respawn: CheckAndRespawn() is called from MissionServer.OnUpdate
//  every 30s. If a trader entity is null or dead, it is silently
//  respawned. No damage hooks required.
// ============================================================

class QT_TraderSpawner
{
    // Static map: entity low ID -> trader definition id
    // Populated at spawn, checked in ActionInteractTrader
    static ref map<int, string> s_entityToTrader   = new map<int, string>();
    static ref map<int, string> s_entityToName      = new map<int, string>();
    static ref map<int, string> s_entityToGreeting  = new map<int, string>();

    private ref array<Object>         m_spawnedNPCs;
    private ref array<ref QT_TraderDef> m_traderDefs; // parallel array to m_spawnedNPCs
    private float                     m_respawnTimer;
    private static const float        RESPAWN_CHECK_INTERVAL = 30.0; // seconds

    void QT_TraderSpawner()
    {
        m_spawnedNPCs  = new array<Object>();
        m_traderDefs   = new array<ref QT_TraderDef>();
        m_respawnTimer = 0;
    }

    void SpawnAll(QT_Config cfg)
    {
        if (!cfg) return;
        foreach (QT_TraderDef def : cfg.TraderNPCPositions)
            SpawnNPC(def);
    }

    void DespawnAll()
    {
        foreach (Object obj : m_spawnedNPCs)
        {
            if (obj) GetGame().ObjectDelete(obj);
        }
        m_spawnedNPCs.Clear();
        m_traderDefs.Clear();
        s_entityToTrader.Clear();
        s_entityToName.Clear();
        s_entityToGreeting.Clear();
    }

    // Called from MissionServer.OnUpdate — checks every 30s and
    // respawns any trader that has gone null or died
    void CheckAndRespawn(float deltaTime)
    {
        m_respawnTimer += deltaTime;
        if (m_respawnTimer < RESPAWN_CHECK_INTERVAL) return;
        m_respawnTimer = 0;

        for (int i = 0; i < m_spawnedNPCs.Count(); i++)
        {
            Object obj = m_spawnedNPCs[i];
            QT_TraderDef def = m_traderDefs[i];
            if (!def) continue;

            bool needsRespawn = false;

            if (!obj)
            {
                needsRespawn = true;
            }
            else
            {
                EntityAI ent = EntityAI.Cast(obj);
                if (ent && !ent.IsAlive()) needsRespawn = true;
            }

            if (!needsRespawn) continue;

            // Remove stale map entries for the old entity
            if (obj)
            {
                int oldId = obj.GetID();
                s_entityToTrader.Remove(oldId);
                s_entityToName.Remove(oldId);
                s_entityToGreeting.Remove(oldId);
            }

            Print("[QuestTrader] Trader '" + def.name + "' is dead/missing - respawning.");

            // Spawn replacement and update slot
            Object replacement = SpawnNPCObject(def);
            m_spawnedNPCs.Set(i, replacement);
        }
    }

    private void SpawnNPC(QT_TraderDef def)
    {
        Object obj = SpawnNPCObject(def);
        m_spawnedNPCs.Insert(obj);
        m_traderDefs.Insert(def);
    }

    // Core spawn logic — returns the spawned object (may be null on failure)
    private Object SpawnNPCObject(QT_TraderDef def)
    {
        string className = ResolveClassName(def.model);

        vector spawnPos = def.position;

        QT_TraderMarker.GetInstance().CreateMarker(def);

        float groundY = GetGame().SurfaceY(spawnPos[0], spawnPos[2]);
        spawnPos[1] = groundY;

        Object obj = GetGame().CreateObject(className, spawnPos, false, true);
        if (!obj)
        {
            Print("[QuestTrader] ERROR - Could not spawn: " + className + " for trader: " + def.id);
            return null;
        }

        obj.SetPosition(spawnPos);
        obj.SetOrientation(def.orientation);

        // Make trader invulnerable via engine flag
        EntityAI npcEnt = EntityAI.Cast(obj);
        if (npcEnt) npcEnt.SetAllowDamage(false);

        // Track this entity as a trader
        int lowId = obj.GetID();
        s_entityToTrader.Insert(lowId, def.id);
        s_entityToName.Insert(lowId, def.name);
        s_entityToGreeting.Insert(lowId, def.greeting);

        Print("[QuestTrader] Spawned trader: " + def.name + " (id=" + lowId + ") at " + spawnPos.ToString());

        // Equip outfit items
        if (def.outfit && def.outfit.Count() > 0)
        {
            if (npcEnt)
            {
                foreach (string itemClass : def.outfit)
                {
                    if (itemClass != "")
                        npcEnt.GetInventory().CreateInInventory(itemClass);
                }
            }
        }

        return obj;
    }

    // --------------------------------------------------------
    //  Check if an object is one of our traders
    // --------------------------------------------------------
    static bool IsTrader(Object obj)
    {
        if (!obj) return false;
        return s_entityToTrader.Contains(obj.GetID());
    }

    static string GetTraderId(Object obj)
    {
        if (!obj) return "";
        int id = obj.GetID();
        if (s_entityToTrader.Contains(id)) return s_entityToTrader.Get(id);
        return "";
    }

    static string GetTraderName(Object obj)
    {
        if (!obj) return "";
        int id = obj.GetID();
        if (s_entityToName.Contains(id)) return s_entityToName.Get(id);
        return "";
    }

    static string GetTraderGreeting(Object obj)
    {
        if (!obj) return "";
        int id = obj.GetID();
        if (s_entityToGreeting.Contains(id)) return s_entityToGreeting.Get(id);
        return "";
    }

    private string ResolveClassName(string model)
    {
        // All valid survivor models — pass through directly if recognised,
        // fall back to SurvivorM_Mirek if the config contains an unknown value.
        switch (model)
        {
            // Female
            case "SurvivorF_Eva":   return model;
            case "SurvivorF_Frida": return model;
            case "SurvivorF_Gabi":  return model;
            case "SurvivorF_Helga": return model;
            case "SurvivorF_Irena": return model;
            case "SurvivorF_Judy":  return model;
            case "SurvivorF_Keiko": return model;
            case "SurvivorF_Linda": return model;
            case "SurvivorF_Maria": return model;
            case "SurvivorF_Naomi": return model;
            case "SurvivorF_Baty":  return model;
            // Male
            case "SurvivorM_Boris":  return model;
            case "SurvivorM_Cyril":  return model;
            case "SurvivorM_Denis":  return model;
            case "SurvivorM_Elias":  return model;
            case "SurvivorM_Francis":return model;
            case "SurvivorM_Guo":    return model;
            case "SurvivorM_Hassan": return model;
            case "SurvivorM_Indar":  return model;
            case "SurvivorM_Jose":   return model;
            case "SurvivorM_Kaito":  return model;
            case "SurvivorM_Lewis":  return model;
            case "SurvivorM_Manua":  return model;
            case "SurvivorM_Mirek":  return model;
            case "SurvivorM_Niki":   return model;
            case "SurvivorM_Oliver": return model;
            case "SurvivorM_Peter":  return model;
            case "SurvivorM_Quinn":  return model;
            case "SurvivorM_Rolf":   return model;
            case "SurvivorM_Seth":   return model;
            case "SurvivorM_Taiki":  return model;
        }
        // Unknown model string — log and fall back to default
        Print("[QuestTrader] WARNING - Unknown model '" + model + "', falling back to SurvivorM_Mirek");
        return "SurvivorM_Mirek";
    }
}
