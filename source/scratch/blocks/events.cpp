#include "events.hpp"
#include "blockExecutor.hpp"
#include "input.hpp"
#include "interpret.hpp"
#include "sprite.hpp"

BlockResult EventBlocks::flagClicked(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult EventBlocks::whenBackdropSwitchesTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult EventBlocks::broadcast(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string name = Scratch::getInputValue(block, "BROADCAST_INPUT", sprite).asString();
    std::cerr << "[BROADCAST-FIRE] " << name << std::endl;
    broadcastQueue.push_back(name);
    return BlockResult::CONTINUE;
}

BlockResult EventBlocks::broadcastAndWait(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (block.repeatTimes == -1) {
        std::cerr << "[BROADCAST-AND-WAIT] " << Scratch::getInputValue(block, "BROADCAST_INPUT", sprite).asString() << std::endl;
    }

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -10;
        BlockExecutor::addToRepeatQueue(sprite, &block);
        block.broadcastsRun = BlockExecutor::runBroadcast(Scratch::getInputValue(block, "BROADCAST_INPUT", sprite).asString());
    }

    bool shouldEnd = true;
    for (auto &[blockPtr, spritePtr] : block.broadcastsRun) {
        if (spritePtr->toDelete) continue;
        if (!spritePtr->blockChains[blockPtr->blockChainID].blocksToRepeat.empty()) {
            shouldEnd = false;
            break;
        }
    }

    if (!shouldEnd) return BlockResult::RETURN;

    block.repeatTimes = -1;
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

BlockResult EventBlocks::whenKeyPressed(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}
