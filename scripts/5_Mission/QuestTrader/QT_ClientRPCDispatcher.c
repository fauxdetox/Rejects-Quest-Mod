// ============================================================
//  QuestTrader | QT_ClientRPCDispatcher.c  (5_Mission)
//  Handles client-side RPC dispatch.
//  MissionGameplay is a 5_Mission type so it's only accessible here.
//  Registered as singleton in MissionGameplay.OnInit().
// ============================================================

class QT_ClientRPCDispatcher extends QT_ClientRPCDispatcherBase
{
    override void DispatchClient(int rpc_type, ParamsReadContext ctx)
    {
        if (!GetGame().IsClient()) return;
        if (!QT_RPCGuard.IsQuestTraderRPC(rpc_type)) return;
        if (!ctx) return;

        MissionGameplay mission = MissionGameplay.Cast(GetGame().GetMission());
        if (mission) mission.QT_HandleClientRPC(rpc_type, ctx);
    }
}
