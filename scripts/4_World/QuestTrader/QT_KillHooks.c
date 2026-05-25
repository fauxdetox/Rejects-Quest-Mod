// ============================================================
//  QuestTrader | QT_KillHooks.c  (scripts/4_World)
//  Kill/destruction attribution para vanilla e mods.
//
//  HIERARQUIA DE CLASSES ENFORCE SCRIPT (DayZ 1.29):
//    Object
//     └─ IEntity
//         └─ EntityAI          ← NÃO modável em 4_World
//             └─ DayZCreatureAI ← NÃO modável em 4_World
//                 ├─ ZombieBase  ← modável ✓ (zumbis, infectados, NPCs)
//                 └─ AnimalBase  ← modável ✓ (animais)
//             └─ ManBase
//                 └─ PlayerBase  ← modável ✓
//
//  ZombieBase cobre infectados e NPCs que herdam dessa base.
//  AnimalBase cobre animais vanilla e modados.
//  ItemBase, BuildingBase, CarScript e BoatScript cobrem alvos destrutíveis.
//
//  Estado de attacker/credit é armazenado no QuestManager via
//  map<int, string> (EntityLowId -> PlayerUID) para evitar guardar
//  referências de PlayerBase que podem ficar inválidas após desconexão.
// ============================================================

class QT_KillHelper
{
    static const int ATTACKER_EXPIRY_SECONDS = 45;
    static const int DEDUP_WINDOW_SECONDS    = 2;

    // --------------------------------------------------------
    //  Resolve o PlayerBase a partir do objeto que causou o dano.
    //  Percorre a hierarquia de parent até encontrar um PlayerBase.
    // --------------------------------------------------------
    static PlayerBase FindKillerPlayer(Object killer)
    {
        if (!killer) return null;

        PlayerBase direct = PlayerBase.Cast(killer);
        if (direct) return direct;

        EntityAI ent = EntityAI.Cast(killer);
        if (!ent) return null;

        PlayerBase fromHierarchy = FindPlayerInHierarchy(ent);
        if (fromHierarchy) return fromHierarchy;

        Object root = ent.GetHierarchyRootPlayer();
        if (root) return PlayerBase.Cast(root);

        return null;
    }

    static PlayerBase FindFromDamageSource(EntityAI source)
    {
        if (!source) return null;

        PlayerBase direct = PlayerBase.Cast(source);
        if (direct) return direct;

        PlayerBase fromHierarchy = FindPlayerInHierarchy(source);
        if (fromHierarchy) return fromHierarchy;

        Object root = source.GetHierarchyRootPlayer();
        if (root) return PlayerBase.Cast(root);

        return null;
    }

    static PlayerBase FindPlayerInHierarchy(EntityAI ent)
    {
        int depth = 0;
        while (ent && depth < 8)
        {
            PlayerBase player = PlayerBase.Cast(ent);
            if (player) return player;

            EntityAI parent = EntityAI.Cast(ent.GetHierarchyParent());
            if (!parent || parent == ent) break;
            ent = parent;
            depth++;
        }
        return null;
    }

    // --------------------------------------------------------
    //  Lógica compartilhada chamada pelos dois hooks abaixo.
    //  Armazena o último atacante via QuestManager (por UID de
    //  string, não por referência ao objeto PlayerBase).
    // --------------------------------------------------------
    static void HandleHitBy(EntityAI self, EntityAI source)
    {
        if (!GetGame().IsServer()) return;
        PlayerBase player = FindFromDamageSource(source);
        if (!player || !player.GetIdentity()) return;
        QT_QuestManager.GetInstance().QT_StoreAttackerUID(self, player.GetIdentity().GetId());
    }

    static void HandleKilled(EntityAI self, Object killer, bool allowPlayerVictim = false)
    {
        if (!GetGame().IsServer()) return;
        if (!allowPlayerVictim && PlayerBase.Cast(self)) return;

        int now = GetGame().GetTime() / 1000;

        if (QT_QuestManager.GetInstance().QT_IsRecentlyKillCredited(self, now, DEDUP_WINDOW_SECONDS))
            return;

        PlayerBase player = null;

        if (killer && killer != self)
            player = FindKillerPlayer(killer);

        if (!player)
            player = QT_QuestManager.GetInstance().QT_ResolveStoredAttacker(self, now, ATTACKER_EXPIRY_SECONDS);

        if (!player || !player.GetIdentity()) return;
        if (player == PlayerBase.Cast(self)) return;

        QT_QuestManager.GetInstance().QT_MarkKillCredited(self, now);
        QT_QuestManager.GetInstance().OnEntityKilled(self, player);
    }

    static void HandleDestroyed(EntityAI self)
    {
        HandleKilled(self, null);
    }
}

// ============================================================
//  Hook 1 – Zumbis, infectados e NPCs customizados de mods.
//  SurvivorBase, Snafu Soldiers, Namalsk Ahrida e outros NPCs
//  de mods geralmente herdam de ZombieBase.
// ============================================================
modded class ZombieBase
{
    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
                           int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        QT_KillHelper.HandleHitBy(this, source);
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }

    override void EEKilled(Object killer)
    {
        super.EEKilled(killer);
        QT_KillHelper.HandleKilled(this, killer);
    }
}

// ============================================================
//  Hook 2 – Animais (vanilla e modados).
// ============================================================
modded class AnimalBase
{
    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
                           int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        QT_KillHelper.HandleHitBy(this, source);
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }

    override void EEKilled(Object killer)
    {
        super.EEKilled(killer);
        QT_KillHelper.HandleKilled(this, killer);
    }
}

// ============================================================
//  Hook 3 - Construções e objetos de mapa com sistema de dano.
// ============================================================
modded class BuildingBase
{
    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
                           int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        QT_KillHelper.HandleHitBy(this, source);
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }

    override void EEKilled(Object killer)
    {
        super.EEKilled(killer);
        QT_KillHelper.HandleKilled(this, killer);
    }

    override void OnDamageDestroyed(int oldLevel)
    {
        super.OnDamageDestroyed(oldLevel);
        QT_KillHelper.HandleDestroyed(this);
    }
}

// ============================================================
//  Hook 4 - Veículos terrestres. Conta quando o veículo alvo é destruído.
// ============================================================
modded class CarScript
{
    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
                           int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        QT_KillHelper.HandleHitBy(this, source);
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }

    override void EEKilled(Object killer)
    {
        super.EEKilled(killer);
        QT_KillHelper.HandleKilled(this, killer);
    }

    override void OnDamageDestroyed(int oldLevel)
    {
        super.OnDamageDestroyed(oldLevel);
        QT_KillHelper.HandleDestroyed(this);
    }
}

// ============================================================
//  Hook 5 - Barcos vanilla 1.29 e derivados.
// ============================================================
modded class BoatScript
{
    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
                           int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        QT_KillHelper.HandleHitBy(this, source);
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }

    override void EEKilled(Object killer)
    {
        super.EEKilled(killer);
        QT_KillHelper.HandleKilled(this, killer);
    }

    override void OnDamageDestroyed(int oldLevel)
    {
        super.OnDamageDestroyed(oldLevel);
        QT_KillHelper.HandleDestroyed(this);
    }
}
