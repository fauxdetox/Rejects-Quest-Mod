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
    private static const float QT_INVENTORY_QUEST_INTERVAL = 2.0;

    override void OnConnect()
    {
        super.OnConnect();
        if (GetGame().IsServer() && GetIdentity())
        {
            QT_QuestManager.GetInstance().OnPlayerConnected(GetIdentity().GetId());
            m_qt_hudScheduled = true;
            // Both HUD update and trader positions are deferred to OnScheduledTick
            // so the client RPC handler is ready before we send anything
        }
    }

    override void OnDisconnect()
    {
        if (GetGame().IsServer() && GetIdentity())
            QT_QuestManager.GetInstance().OnPlayerDisconnected(GetIdentity().GetId());
        super.OnDisconnect();
    }

    override void OnScheduledTick(float deltaTime)
    {
        super.OnScheduledTick(deltaTime);
        if (m_qt_hudScheduled && GetGame().IsServer() && GetIdentity())
        {
            m_qt_hudScheduled = false;
            QT_RPCManager.SendHUDUpdate(this);
            QT_RPCManager.SendTraderPositions(this);
            m_qt_lastInventoryQuestSignature = QT_QuestManager.GetInstance().BuildInventoryQuestSignature(this);
            m_qt_lastReadyInventoryQuestSignature = QT_QuestManager.GetInstance().BuildReadyInventoryQuestSignature(this);
            m_qt_inventorySignatureInitialised = true;
            m_qt_inventoryQuestTimer = 0;
            return;
        }

        if (GetGame().IsServer() && GetIdentity())
        {
            m_qt_inventoryQuestTimer += deltaTime;
            if (m_qt_inventoryQuestTimer >= QT_INVENTORY_QUEST_INTERVAL)
            {
                m_qt_inventoryQuestTimer = 0;
                QT_QuestManager mgr = QT_QuestManager.GetInstance();
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
                        QT_RPCManager.SendToast(this, "#QuestTrader_QUEST_READY_TURN_IN", QT_ToastType.COMPLETE);
                    m_qt_lastReadyInventoryQuestSignature = readySig;
                }
            }
        }
    }

}
