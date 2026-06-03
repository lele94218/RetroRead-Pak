#include "ui/SceneManager.h"

#include "ui/Scene.h"

SceneManager::~SceneManager() = default;

void SceneManager::setRoot(std::unique_ptr<Scene> scene) {
    current_ = std::move(scene);
    if (current_) {
        current_->onEnter();
        renderRequested_ = true;
    }
}

void SceneManager::replace(std::unique_ptr<Scene> scene) {
    if (current_) {
        current_->onExit();
    }

    pendingDestroy_ = std::move(current_);
    current_ = std::move(scene);
    wasReplaced_ = true;
    if (current_) {
        current_->onEnter();
        renderRequested_ = true;
    }
}

void SceneManager::update(float dt) {
    wasReplaced_ = false;
    if (current_) {
        current_->update(dt);
    }
    pendingDestroy_.reset();
}

void SceneManager::render(Renderer& renderer) {
    if (current_) {
        current_->render(renderer);
    }
}

bool SceneManager::shouldRenderContinuously() const {
    return current_ != nullptr && current_->shouldRenderContinuously();
}

bool SceneManager::consumeRenderRequest() {
    bool requested = renderRequested_;
    renderRequested_ = false;
    if (current_ != nullptr && current_->consumeRenderRequest()) {
        requested = true;
    }
    return requested;
}
