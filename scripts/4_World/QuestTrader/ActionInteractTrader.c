// ============================================================
//  QuestTrader | ActionInteractTrader.c  (v2.3)
//  Kept for registration - actual interaction is handled
//  via proximity check in QT_PlayerBase.OnScheduledTick
// ============================================================

class ActionInteractTrader : ActionBase
{
    void ActionInteractTrader() { m_Text = "#QuestTrader_ACTION_TALK"; }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        return false; // Disabled - proximity system handles interaction
    }
}
