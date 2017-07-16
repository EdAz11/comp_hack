/**
 * @file server/channel/src/packets/game/CashBalance.cpp
 * @ingroup channel
 *
 * @author HACKfrost
 *
 * @brief Request from the current account's total CP.
 *
 * This file is part of the Channel Server (channel).
 *
 * Copyright (C) 2012-2016 COMP_hack Team <compomega@tutanota.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Packets.h"

// libcomp Includes
#include <ManagerPacket.h>
#include <Packet.h>
#include <PacketCodes.h>

// object Includes
#include <Account.h>
#include <Character.h>

// channel Includes
#include "ChannelServer.h"

using namespace channel;

bool Parsers::CashBalance::Parse(libcomp::ManagerPacket *pPacketManager,
    const std::shared_ptr<libcomp::TcpConnection>& connection,
    libcomp::ReadOnlyPacket& p) const
{
    if(p.Size() != 0)
    {
        return false;
    }

    auto server = std::dynamic_pointer_cast<ChannelServer>(pPacketManager->GetServer());
    auto client = std::dynamic_pointer_cast<ChannelClientConnection>(connection);
    auto state = client->GetClientState();
    auto cState = state->GetCharacterState();
    auto character = cState->GetEntity();

    // Always reload the account to get the latest CP value
    auto account = libcomp::PersistentObject::LoadObjectByUUID<objects::Account>(
        server->GetLobbyDatabase(), character->GetAccount().GetUUID(), true);

    libcomp::Packet reply;
    reply.WritePacketCode(ChannelToClientPacketCode_t::PACKET_CASH_BALANCE);
    reply.WriteS32Little((int32_t)account->GetCP());
    reply.WriteS32Little(0);

    client->SendPacket(reply);

    return true;
}
