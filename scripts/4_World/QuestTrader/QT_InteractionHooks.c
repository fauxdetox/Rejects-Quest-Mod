// ============================================================
//  QuestTrader | QT_InteractionHooks.c
//  Counts vanilla object interactions for INTERACT quests.
// ============================================================

modded class ActionBase
{
    override void OnStartServer(ActionData action_data)
    {
        super.OnStartServer(action_data);

        if (!GetGame().IsServer()) return;
        if (!action_data || !action_data.m_Player || !action_data.m_Target) return;

        Object targetObject = action_data.m_Target.GetObject();
        if (targetObject)
            QT_QuestManager.GetInstance().CompleteInteractionObjectiveFromObject(action_data.m_Player, targetObject);

        Object parentObject = action_data.m_Target.GetParent();
        if (parentObject && parentObject != targetObject)
            QT_QuestManager.GetInstance().CompleteInteractionObjectiveFromObject(action_data.m_Player, parentObject);
    }
}

modded class ItemBase
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

    override void EEItemAttached(EntityAI item, string slot_name)
    {
        super.EEItemAttached(item, slot_name);
        QT_CheckQuestInventoryInteraction();
    }

    override void EEItemDetached(EntityAI item, string slot_name)
    {
        super.EEItemDetached(item, slot_name);
        QT_CheckQuestInventoryInteraction();
    }

    override void OnItemLocationChanged(EntityAI old_owner, EntityAI new_owner)
    {
        super.OnItemLocationChanged(old_owner, new_owner);

        if (old_owner)
            QT_CheckQuestInventoryInteractionOwner(old_owner);

        if (new_owner && new_owner != old_owner)
            QT_CheckQuestInventoryInteractionOwner(new_owner);
    }

    private void QT_CheckQuestInventoryInteraction()
    {
        if (!GetGame().IsServer()) return;

        PlayerBase player = PlayerBase.Cast(GetHierarchyRootPlayer());
        if (!player) return;

        QT_QuestManager.GetInstance().CompleteInteractionObjectiveFromObject(player, this);
    }

    private void QT_CheckQuestInventoryInteractionOwner(EntityAI owner)
    {
        if (!GetGame().IsServer()) return;
        if (!owner) return;

        PlayerBase player = PlayerBase.Cast(owner.GetHierarchyRootPlayer());
        if (!player) return;

        QT_QuestManager.GetInstance().CompleteInteractionObjectiveFromObject(player, owner);
    }
}
