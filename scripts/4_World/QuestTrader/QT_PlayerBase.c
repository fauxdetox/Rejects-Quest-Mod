// ============================================================
//  QuestTrader | QT_PlayerBase.c  (v3.2)
//  Server-side only: persistence hooks + HUD push on connect.
//  Interaction is handled entirely client-side via
//  MissionGameplay.OnUpdate proximity + F key detection.
//  Trader invulnerability handled via SetAllowDamage(false)
//  in QT_TraderSpawner.c on spawn.
// ============================================================

modded class PlayerBase
{
    private bool m_qt_hudScheduled = false;
    private float m_qt_inventoryQuestTimer = 0;
    private string m_qt_lastInventoryQuestSignature = "";
    private string m_qt_lastReadyInventoryQuestSignature = "";
    private bool m_qt_inventorySignatureInitialised = false;
    private static const float QT_INVENTORY_QUEST_INTERVAL = 5.0;

    override void OnConnect()
    {
        super.OnConnect();
        if (GetGame().IsServer() && GetIdentity())
        {
            QT_QuestManager.GetInstance().RegisterOnlinePlayer(this, GetIdentity());
            m_qt_hudScheduled = true;
            GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(QT_SendInitialQuestSync, Math.RandomInt(8000, 15000), false);
            // Delay the first non-critical sync so connect bursts do not stall the server.
        }
    }

    override void OnDisconnect()
    {
        // MissionServer handles the actual disconnect event. Doing it here can
        // race with respawn/relog character swaps and remove an active player
        // from the admin online list.
        m_qt_hudScheduled = false;
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(QT_SendInitialQuestSync);
        super.OnDisconnect();
    }

    override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
                           int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
    {
        QT_KillHelper.HandleHitBy(this, source);
        super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
    }

    override void EEKilled(Object killer)
    {
        super.EEKilled(killer);
        QT_KillHelper.HandleKilled(this, killer, true);
    }

    override void OnScheduledTick(float deltaTime)
    {
        super.OnScheduledTick(deltaTime);
        if (GetGame().IsServer() && GetIdentity())
        {
            m_qt_inventoryQuestTimer += deltaTime;
            if (m_qt_inventoryQuestTimer >= QT_INVENTORY_QUEST_INTERVAL)
            {
                m_qt_inventoryQuestTimer = 0;
                QT_QuestManager mgr = QT_QuestManager.GetInstance();
                if (!mgr.HasActiveInventoryQuest(GetIdentity().GetId()))
                {
                    m_qt_lastInventoryQuestSignature = "";
                    m_qt_lastReadyInventoryQuestSignature = "";
                    return;
                }
                string sig = mgr.BuildInventoryQuestSignature(this);
                string readySig = mgr.BuildReadyInventoryQuestSignature(this);
                if (!m_qt_inventorySignatureInitialised)
                {
                    m_qt_lastInventoryQuestSignature = sig;
                    m_qt_lastReadyInventoryQuestSignature = readySig;
                    m_qt_inventorySignatureInitialised = true;
                }
                else if (sig != m_qt_lastInventoryQuestSignature)
                {
                    m_qt_lastInventoryQuestSignature = sig;
                    QT_RPCManager.SendHUDUpdate(this);

                    if (readySig != "" && readySig != m_qt_lastReadyInventoryQuestSignature)
                    {
                        QT_RPCManager.SendToast(this, "#QuestTrader_QUEST_READY_TURN_IN", QT_ToastType.COMPLETE);
                    }
                    m_qt_lastReadyInventoryQuestSignature = readySig;
                }
            }
        }
    }

    private void QT_SendInitialQuestSync()
    {
        if (!m_qt_hudScheduled) return;
        if (!GetGame().IsServer() || !GetIdentity()) return;

        m_qt_hudScheduled = false;
        QT_RPCManager.SendHUDUpdate(this);
        m_qt_lastInventoryQuestSignature = QT_QuestManager.GetInstance().BuildInventoryQuestSignature(this);
        m_qt_lastReadyInventoryQuestSignature = QT_QuestManager.GetInstance().BuildReadyInventoryQuestSignature(this);
        m_qt_inventorySignatureInitialised = true;
        m_qt_inventoryQuestTimer = 0;
    }

}
