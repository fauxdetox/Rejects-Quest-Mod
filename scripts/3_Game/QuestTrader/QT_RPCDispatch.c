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

modded class DayZGame
{
    override void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, target, rpc_type, ctx);

        if (rpc_type < 9100 || rpc_type > 9119) return;

        if (QT_RPCDispatcherBase.s_instance)
            QT_RPCDispatcherBase.s_instance.Dispatch(sender, target, rpc_type, ctx);

        if (QT_ClientRPCDispatcherBase.s_instance)
            QT_ClientRPCDispatcherBase.s_instance.DispatchClient(rpc_type, ctx);
    }
}
