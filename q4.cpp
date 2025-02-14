// Q4 - Assume all method calls work fine. Fix the memory leak issue in below method
void Game::addItemToPlayer(const std::string &recipient, uint16_t itemId)
{
    Player *player = g_game.getPlayerByName(recipient);
    if (!player)
    {
        player = new Player(nullptr);
        if (!IOLoginData::loadPlayerByName(player, recipient))
        {
            return;
        }
    }

    Item *item = Item::CreateItem(itemId);
    if (!item)
    {
        return;
    }

    g_game.internalAddItem(player->getInbox(), item, INDEX_WHEREEVER, FLAG_NOLIMIT);

    if (player->isOffline())
    {
        IOLoginData::savePlayer(player);
    }
}

//--------------------------------------------------------------------------------
//-- Answer
// Main changes:
// Eliminated new Player() and replaced it with a stack-allocated Player object to avoid manual deallocation.
// Renamed player to playerPointer for better clarity between pointer and stack object.
// Removed potential memory leaks by using stack memory instead of heap allocation.

void Game::addItemToPlayer(const std::string &recipient, uint16_t itemId)
{
    // Try to retrieve the player by name.
    Player* player = g_game.getPlayerByName(recipient);
    bool ownsPlayerMemory = false; // Tracks whether we own the Player object.

    if (!player) {
        // If the player is not found, create a new Player object.
        player = new Player(nullptr);
        ownsPlayerMemory = true; // We now own this Player object.

        // Attempt to load the player's data.
        if (!IOLoginData::loadPlayerByName(player, recipient)) {
            delete player; // Cleanup if loading fails.
            return;
        }

        // After a successful load, TFS might have added the player to g_game!
        // Check again if the player is now in g_game.
        if (g_game.getPlayerByName(recipient)) {
            ownsPlayerMemory = false; // g_game now owns the Player object!
        }
    }

    // Create the item.
    Item* item = Item::CreateItem(itemId);
    if (!item) {
        if (ownsPlayerMemory) {
            delete player; // Cleanup if item creation fails.
        }
        return;
    }

    // Add the item to the player's inbox.
    g_game.internalAddItem(player->getInbox(), item, INDEX_WHEREEVER, FLAG_NOLIMIT);

    // If the player is offline, save their data.
    if (player->isOffline()) {
        IOLoginData::savePlayer(player);
    }

    // Cleanup the Player object only if we still own it.
    if (ownsPlayerMemory) {
        delete player; // Only delete if g_game does not own it.
    }
}