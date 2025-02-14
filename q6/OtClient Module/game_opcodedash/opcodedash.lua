-- Initialize the signal reader.
function init()
    print("INITIALZE")
    ProtocolGame.registerExtendedOpcode(999, onReceiveOpcode) -- Dash OPCode
end

-- Cleanup everything.
function terminate()
    ProtocolGame.unregisterExtendedOpcode(999)
end

-- Handle incoming opcode.
function onReceiveOpcode(protocol, opcode, packet)
    if opcode == 999 then
        local values = {} -- Container for parsed data.

        -- Extract data from the packet.
        for word in packet:gmatch("([^,]+)") do
            table.insert(values, word)
        end

        -- Fetch the creature that performed the dash.
        local creature = g_map.getCreatureById(tonumber(values[1]))

        if creature then
            -- Start or end the dash based on the action.
            if values[2] == "sd" then
                creature:startDash()
            elseif values[2] == "ed" then
                creature:endDash()
            end
        else
            print("Creature not found for ID: " .. tostring(values[1]))
        end
    end
end