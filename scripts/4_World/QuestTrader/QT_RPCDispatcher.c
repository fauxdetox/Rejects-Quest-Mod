// ============================================================
//  QuestTrader | QT_RPCDispatcher.c  (4_World)
//
//  For Server->Client RPCs: target is the player object.
//  For Client->Server RPCs: target is null; use sender identity
//  to find the PlayerBase via GetGame().GetPlayers().
// ============================================================

class QT_RPCDispatcher extends QT_RPCDispatcherBase
{
    override void Dispatch(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
    {
        if (!GetGame().IsServer()) return;
        if (!QT_RPCGuard.IsQuestTraderRPC(rpc_type)) return;
        if (!ctx) return;

        // S->C: target is the player directly
        PlayerBase player = PlayerBase.Cast(target);

        // C->S: target is null - find player by identity
        if (!player && sender)
        {
            array<Man> players = new array<Man>();
            GetGame().GetPlayers(players);
            foreach (Man m : players)
            {
                PlayerBase pb = PlayerBase.Cast(m);
                if (pb && pb.GetIdentity() && pb.GetIdentity().GetId() == sender.GetId())
                {
                    player = pb;
                    break;
                }
            }
        }

        if (player)
            QT_RPCManager.HandleRPC(player, rpc_type, ctx);
    }
}
