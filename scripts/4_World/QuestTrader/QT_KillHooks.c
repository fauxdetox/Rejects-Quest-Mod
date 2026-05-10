// ============================================================
//  QuestTrader | QT_KillHooks.c  (4_World)
//  Kill attribution with bleed-out support.
//
//  Problem: EEKilled can fire TWICE for bleed-out deaths:
//    1. On fatal hit (killer = weapon/projectile) - correctly attributed
//    2. On bleed-out tick (killer = the animal itself) - wrongly unattributed
//
//  Fix: store last attacker with a timestamp. Credit any kill within
//  30 seconds of the last player hit, even if EEKilled comes from
//  bleed-out. Don't clear attacker after crediting - let it expire
//  naturally so double-fire doesn't double-credit.
//
//  m_qt_creditedKillTime: epoch of last credited kill. If the same
//  animal EEKilled fires again within 2s, skip (already credited).
// ============================================================

class QT_KillHelper
{
    static PlayerBase FindKillerPlayer(Object killer)
    {
        if (!killer) return null;

        // Direct player (melee)
        PlayerBase direct = PlayerBase.Cast(killer);
        if (direct) return direct;

        // Walk up parent chain: projectile -> weapon -> hands -> player
        EntityAI ent = EntityAI.Cast(killer);
        while (ent)
        {
            PlayerBase p = PlayerBase.Cast(ent);
            if (p) return p;
            EntityAI parent = EntityAI.Cast(ent.GetHierarchyParent());
            if (!parent) break;
            ent = parent;
        }
        return null;
    }

    // Try to find a player from a DamageSource / projectile info
    static PlayerBase FindFromDamageSource(EntityAI source)
    {
        if (!source) return null;
        PlayerBase p = PlayerBase.Cast(source);
        if (p) return p;
        EntityAI ent = source;
        while (ent)
        {
            p = PlayerBase.Cast(ent);
            if (p) return p;
            EntityAI parent = EntityAI.Cast(ent.GetHierarchyParent());
            if (!parent) break;
            ent = parent;
        }
        return null;
    }
}

modded class AnimalBase
{
    private PlayerBase m_qt_lastAttacker;
    private int        m_qt_lastHitTime;      // GetGame().GetTime() / 1000 of last player hit
    private int        m_qt_creditedKillTime; // epoch of last credited kill (dedup guard)

    private static const int ATTACKER_EXPIRY_SECONDS = 30; // bleed-out window
    private static const int DEDUP_WINDOW_SECONDS    = 2;  // ignore second EEKilled within 2s

    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
                           int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        // Store attacker BEFORE super call - super may trigger EEKilled for fatal hits
        if (GetGame().IsServer())
        {
            PlayerBase player = QT_KillHelper.FindFromDamageSource(source);
            if (player)
            {
                m_qt_lastAttacker = player;
                m_qt_lastHitTime  = GetGame().GetTime() / 1000;
            }
        }
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }

    override void EEKilled(Object killer)
    {
        super.EEKilled(killer);
        if (!GetGame().IsServer()) return;

        int now = GetGame().GetTime() / 1000;

        // Dedup guard: if we already credited this kill in the last 2 seconds, skip
        if (m_qt_creditedKillTime > 0 && (now - m_qt_creditedKillTime) < DEDUP_WINDOW_SECONDS)
        {
            Print("[QuestTrader] Kill dedup skipped: " + GetType());
            return;
        }

        PlayerBase player = null;

        // 1. Try to resolve player from the killer parameter directly
        //    Skip if killer == this (animal is its own killer = bleed-out)
        if (killer != this)
            player = QT_KillHelper.FindKillerPlayer(killer);

        // 2. Fall back to stored attacker if still within the bleed-out window
        if (!player && m_qt_lastAttacker)
        {
            if ((now - m_qt_lastHitTime) <= ATTACKER_EXPIRY_SECONDS)
                player = m_qt_lastAttacker;
        }

        // 3. Last resort: killer IS a projectile - get its owner via GetHierarchyRootPlayer
        if (!player && killer != this)
        {
            EntityAI killerEnt = EntityAI.Cast(killer);
            if (killerEnt)
            {
                Object root = killerEnt.GetHierarchyRootPlayer();
                if (root) player = PlayerBase.Cast(root);
            }
        }

        if (player)
        {
            m_qt_creditedKillTime = now;
            Print("[QuestTrader] Kill credited: " + GetType() + " -> " + player.GetIdentity().GetName());
            QT_QuestManager.GetInstance().OnEntityKilled(this, player);
        }
        else
        {
            Print("[QuestTrader] Kill NOT credited: " + GetType() + " killer=" + killer.GetType());
        }
    }
}

modded class ZombieBase
{
    private PlayerBase m_qt_lastAttacker;
    private int        m_qt_lastHitTime;
    private int        m_qt_creditedKillTime;

    private static const int ATTACKER_EXPIRY_SECONDS = 30;
    private static const int DEDUP_WINDOW_SECONDS    = 2;

    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
                           int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
        if (!GetGame().IsServer()) return;
        PlayerBase player = QT_KillHelper.FindFromDamageSource(source);
        if (player)
        {
            m_qt_lastAttacker = player;
            m_qt_lastHitTime  = GetGame().GetTime() / 1000;
        }
    }

    override void EEKilled(Object killer)
    {
        super.EEKilled(killer);
        if (!GetGame().IsServer()) return;

        int now = GetGame().GetTime() / 1000;

        if (m_qt_creditedKillTime > 0 && (now - m_qt_creditedKillTime) < DEDUP_WINDOW_SECONDS)
            return;

        PlayerBase player = null;

        if (killer != this)
            player = QT_KillHelper.FindKillerPlayer(killer);

        if (!player && m_qt_lastAttacker)
        {
            if ((now - m_qt_lastHitTime) <= ATTACKER_EXPIRY_SECONDS)
                player = m_qt_lastAttacker;
        }

        if (player)
        {
            m_qt_creditedKillTime = now;
            QT_QuestManager.GetInstance().OnEntityKilled(this, player);
        }
    }
}
