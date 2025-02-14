local jump_window
local jumpButton
local event
local moveDirection = -2 -- Initial movement direction: right to left.

-- Setup everything.
function init()
    g_keyboard.bindKeyDown('Ctrl+J', toggle) -- Set the keybind to open the window.

    jump_window = g_ui.displayUI('game_jumpWindow') -- Load the menu data.
    jump_window:hide()

    jumpButton = jump_window:getChildById('jumpButton') -- Find the button.
end

-- Cleanup everything.
function terminate()
    g_keyboard.unbindKeyDown('Ctrl+J') -- Unbind the keybind.

    jump_window:destroy() -- Destroy the jump window.
    jumpButton:destroy() -- Destroy the jump button.

    removeEvent(event) -- Remove any running event.
end

function show()
    -- Check if the game is online.
    if not g_game.isOnline() then
        return
    end

    -- Show the window and set focus.
    jump_window:show()
    jump_window:raise()
    jump_window:focus()

    -- Center the button every time the window is shown.
    local jump_windowPaddingRect = jump_window:getPaddingRect()

    jumpButton:setPosition({
        x = jump_windowPaddingRect.x + jump_windowPaddingRect.width / 2,
        y = jump_windowPaddingRect.y + jump_windowPaddingRect.height / 2
    })

    -- Create the event that moves the button and keeps it inside the window's bounds.
    event = cycleEvent(function()
        local jump_windowPaddingRect = jump_window:getPaddingRect()
        local jumpButtonRect = jumpButton:getRect()

        -- Move the button horizontally.
        local newX = jumpButtonRect.x + moveDirection

        -- Check horizontal bounds.
        if newX < jump_windowPaddingRect.x then
            -- If it hits the left edge:
            -- Reappear at the right edge and change the Y position.
            newX = jump_windowPaddingRect.x + jump_windowPaddingRect.width - jumpButtonRect.width

            -- Generate a new Y position within the vertical bounds of the window.
            local newY = math.random(jump_windowPaddingRect.y + 20, jump_windowPaddingRect.y + jump_windowPaddingRect.height - jumpButtonRect.height)

            -- Update the button's position.
            jumpButton:setPosition({ x = newX, y = newY })
        else
            -- Otherwise, only update the horizontal position.
            jumpButton:setPosition({ x = newX, y = jumpButtonRect.y })
        end
    end, 1000 / 60) -- Run the event at 60 frames per second.
end

function hide()
    -- Hide the window and remove the event.
    jump_window:hide()

    removeEvent(event)
end

function toggle()
    -- Toggle the visibility of the window.
    if jump_window:isVisible() then
        hide()
    else
        show()
    end
end

-- Function that gets called "onClick".
function moveButton()
    -- Get the padding area of the window, which defines its usable bounds.
    local jump_windowPaddingRect = jump_window:getPaddingRect()
    -- Get the button's current bounding rectangle.
    local jumpButtonRect = jumpButton:getRect()

    -- Generate a new random Y position within the window's vertical bounds.
    local newY = math.random(jump_windowPaddingRect.y + 20, jump_windowPaddingRect.y + jump_windowPaddingRect.height - jumpButtonRect.height)

    -- Set the X position to the far right of the window.
    local newX = jump_windowPaddingRect.x + jump_windowPaddingRect.width - jumpButtonRect.width

    -- Move the button to the new position.
    jumpButton:setPosition({ x = newX, y = newY })
end