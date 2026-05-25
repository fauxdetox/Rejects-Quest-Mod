// ============================================================
//  QuestTrader | QT_RPCDispatch.c  (3_Game)
//
//  Client->Server RPCs arrive with target=null and sender=identity.
//  We pass the sender identity to the 4_World dispatcher so it
//  can look up the PlayerBase from the identity there.
// ============================================================

class QT_RPCDispatcherBase
{
    static ref QT_RPCDispatcherBase s_instance;

    // sender is always available; target may be null for C->S RPCs
    void Dispatch(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
    {
        // 4_World subclass handles server side
    }
}

class QT_ClientRPCDispatcherBase
{
    static ref QT_ClientRPCDispatcherBase s_instance;

    void DispatchClient(int rpc_type, ParamsReadContext ctx)
    {
        // 5_Mission subclass handles client side
    }
}

class QT_RPCGuard
{
    static bool IsQuestTraderRPC(int rpc_type)
    {
        if (rpc_type == 9100) return true;
        if (rpc_type == 9101) return true;
        if (rpc_type == 9102) return true;
        if (rpc_type == 9103) return true;
        if (rpc_type == 9104) return true;
        if (rpc_type == 9105) return true;
        if (rpc_type == 9106) return true;
        if (rpc_type == 9107) return true;
        if (rpc_type == 9108) return true;
        if (rpc_type == 9109) return true;
        if (rpc_type == 9110) return true;
        if (rpc_type == 9111) return true;
        if (rpc_type == 9113) return true;
        if (rpc_type == 9114) return true;
        if (rpc_type == 9115) return true;
        if (rpc_type == 9116) return true;
        if (rpc_type == 9117) return true;
        if (rpc_type == 9118) return true;
        if (rpc_type == 9119) return true;
        if (rpc_type == 9120) return true;
        if (rpc_type == 9121) return true;
        if (rpc_type == 9122) return true;
        return false;
    }
}

modded class DayZGame
{
    override void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
    {
        if (!QT_RPCGuard.IsQuestTraderRPC(rpc_type))
        {
            super.OnRPC(sender, target, rpc_type, ctx);
            return;
        }

        if (!ctx) return;

        bool handled = false;
        if (GetGame().IsServer() && !target && QT_RPCDispatcherBase.s_instance)
        {
            QT_RPCDispatcherBase.s_instance.Dispatch(sender, target, rpc_type, ctx);
            handled = true;
        }

        if (!handled && GetGame().IsClient() && QT_ClientRPCDispatcherBase.s_instance)
        {
            QT_ClientRPCDispatcherBase.s_instance.DispatchClient(rpc_type, ctx);
            handled = true;
        }

        if (!handled && GetGame().IsServer() && QT_RPCDispatcherBase.s_instance)
            QT_RPCDispatcherBase.s_instance.Dispatch(sender, target, rpc_type, ctx);
    }
}
