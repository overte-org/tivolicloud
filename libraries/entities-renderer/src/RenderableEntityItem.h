//
//  RenderableEntityItem.h
//  interface/src
//


#ifndef hifi_RenderableEntityItem_h
#define hifi_RenderableEntityItem_h

#include <render/Scene.h>
#include <EntityItem.h>
#include <Sound.h>
#include "AbstractViewStateInterface.h"
#include "EntitiesRendererLogging.h"
#include <graphics-scripting/Forward.h>
#include <RenderHifi.h>
#include "EntityTreeRenderer.h"

class EntityTreeRenderer;

namespace render { namespace entities {

// Base class for all renderable entities
class EntityRenderer : public QObject, public std::enable_shared_from_this<EntityRenderer>, public PayloadProxyInterface, protected ReadWriteLockable, public scriptable::ModelProvider {
    Q_OBJECT

    using Pointer = std::shared_ptr<EntityRenderer>;

public:
    static void initEntityRenderers();
    static Pointer addToScene(EntityTreeRenderer& renderer, const EntityItemPointer& entity, const ScenePointer& scene, Transaction& transaction);

    // Allow classes to override this to interact with the user
    virtual bool wantsHandControllerPointerEvents() const { return false; }
    virtual bool wantsKeyboardFocus() const { return false; }
    virtual void setProxyWindow(QWindow* proxyWindow) {}
    virtual QObject* getEventHandler() { return nullptr; }
    virtual void emitScriptEvent(const QVariant& message) {}
    virtual void sendToQml(const QVariant& message) {}
    const EntityItemPointer& getEntity() const { return _entity; }
    const ItemID& getRenderItemID() const { return _renderItemID; }

    const SharedSoundPointer& getCollisionSound() { return _collisionSound; }
    void setCollisionSound(const SharedSoundPointer& sound) { _collisionSound = sound; }

    // Handlers for rendering events... executed on the main thread, only called by EntityTreeRenderer, 
    // cannot be overridden or accessed by subclasses
    virtual void updateInScene(const ScenePointer& scene, Transaction& transaction) final;
    virtual bool addToScene(const ScenePointer& scene, Transaction& transaction) final;
    virtual void removeFromScene(const ScenePointer& scene, Transaction& transaction);

    const uint64_t& getUpdateTime() const { return _updateTime; }

    virtual void addMaterial(graphics::MaterialLayer material, const std::string& parentMaterialName);
    virtual void removeMaterial(graphics::MaterialPointer material, const std::string& parentMaterialName);

    virtual scriptable::ScriptableModelBase getScriptableModel() override { return scriptable::ScriptableModelBase(); }

    static glm::vec4 calculatePulseColor(const glm::vec4& color, const PulsePropertyGroup& pulseProperties, quint64 start);
    static glm::vec3 calculatePulseColor(const glm::vec3& color, const PulsePropertyGroup& pulseProperties, quint64 start);

    virtual uint32_t metaFetchMetaSubItems(ItemIDs& subItems) const override;
    virtual Item::Bound getBound() override;

    enum ZoneCullingState {
        ZoneCull_Inactive,
        ZoneCull_Culled,
        ZoneCull_Skipped
    };

    ZoneCullingState _zoneCullState{ ZoneCullingState::ZoneCull_Inactive };
    ZoneCullingState _prevZoneCullState{ ZoneCullingState::ZoneCull_Inactive };
    virtual bool evaluateEntityZoneCullState(const EntityItemPointer& entity);//, QVector<QUuid>& skiplist);

    virtual void setStaticUpdateTime(quint64 time) { _staticUpdateTime = time; }
    virtual quint64 getStaticUpdateTime() { return _staticUpdateTime; }

public slots:
    // If an entity is static, it will not be receiving regular updates from the ETR update loop
    // specialUpdateRequest listens in case the entity sits outside the ETR loop & needs a state change 
   void handleSpecialUpdate(); // so the renderer can request the entity to update itself.


protected:
    virtual bool needsRenderUpdateFromEntity() const final { return needsRenderUpdateFromEntity(_entity); }
    virtual void onAddToScene(const EntityItemPointer& entity);
    virtual void onRemoveFromScene(const EntityItemPointer& entity);

    EntityRenderer(const EntityItemPointer& entity);
    ~EntityRenderer();

    // Implementing the PayloadProxyInterface methods
    virtual ItemKey getKey() override;
    virtual ShapeKey getShapeKey() override;
    virtual void render(RenderArgs* args) override final;
    virtual render::hifi::Tag getTagMask() const;
    virtual render::hifi::Layer getHifiRenderLayer() const;

    // Returns true if the item in question needs to have updateInScene called because of internal rendering state changes
    virtual bool needsRenderUpdate() const;

    // Returns true if the item in question needs to have updateInScene called because of changes in the entity
    virtual bool needsRenderUpdateFromEntity(const EntityItemPointer& entity) const;

    // Will be called on the main thread from updateInScene.  This can be used to fetch things like 
    // network textures or model geometry from resource caches
    virtual void doRenderUpdateSynchronous(const ScenePointer& scene, Transaction& transaction, const EntityItemPointer& entity);

    // Will be called by the lambda posted to the scene in updateInScene.  
    // This function will execute on the rendering thread, so you cannot use network caches to fetch
    // data in this method if using multi-threaded rendering
    
    virtual void doRenderUpdateAsynchronous(const EntityItemPointer& entity) { }

    // Called by the `render` method after `needsRenderUpdate`
    virtual void doRender(RenderArgs* args) = 0;

    virtual bool isFading() const { return _isFading; }
    virtual void updateModelTransformAndBound();
    virtual bool isTransparent() const { return _isFading ? Interpolate::calculateFadeRatio(_fadeStartTime) < 1.0f : false; }
    inline bool isValidRenderItem() const { return _renderItemID ? !Item::INVALID_ITEM_ID : false; }

    virtual void setIsVisibleInSecondaryCamera(bool value) { _isVisibleInSecondaryCamera = value; }
    virtual void setRenderLayer(RenderLayer value) { _renderLayer = value; }
    virtual void setPrimitiveMode(PrimitiveMode value) { _primitiveMode = value; }
    virtual void setCullWithParent(bool value) { _cullWithParent = value; }

    virtual bool wasPreviouslyVisible() const { return _previouslyVisible; }
    virtual void setPreviouslyVisible(bool value) { _previouslyVisible = value; }

signals:
    void requestRenderUpdate(); // so entity can call out to the renderer to re-render it



protected:
    template<typename T>
    std::shared_ptr<T> asTypedEntity() { return std::static_pointer_cast<T>(_entity); }

    static void makeStatusGetters(const EntityItemPointer& entity, Item::Status::Getters& statusGetters);
    const Transform& getModelTransform() const;

    Item::Bound _bound;
    SharedSoundPointer _collisionSound;
    QUuid _changeHandlerId;
    ItemID _renderItemID{ Item::INVALID_ITEM_ID };
    uint64_t _fadeStartTime{ usecTimestampNow() };
    uint64_t _updateTime{ 0 }; // used when sorting/throttling render updates
    uint64_t _staticUpdateTime{ 0 }; 


    bool _isFading { EntityTreeRenderer::getEntitiesShouldFadeFunction()() };
    bool _prevIsTransparent { false };
    bool _visible{ false };
    bool _previouslyVisible{ false };
    bool _isVisibleInSecondaryCamera { false };
    bool _canCastShadow { false };
    

    bool _cullWithParent { false };
    RenderLayer _renderLayer { RenderLayer::WORLD };
    PrimitiveMode _primitiveMode { PrimitiveMode::SOLID };
    bool _cauterized { false };
    bool _moving { false };
    Transform _renderTransform;

    std::unordered_map<std::string, graphics::MultiMaterial> _materials;
    std::mutex _materialsLock;

    quint64 _created;

    // The base class relies on comparing the model transform to the entity transform in order 
    // to trigger an update, so the member must not be visible to derived classes as a modifiable
    // transform
    Transform _modelTransform;
    // The rendering code only gets access to the entity in very specific circumstances
    // i.e. to see if the rendering code needs to update because of a change in state of the 
    // entity.  This forces all the rendering code itself to be independent of the entity
    const EntityItemPointer _entity;
};

template <typename T>
class TypedEntityRenderer : public EntityRenderer {
    using Parent = EntityRenderer;

public:
    TypedEntityRenderer(const EntityItemPointer& entity) : Parent(entity), _typedEntity(asTypedEntity<T>()) {}

protected:
    using TypedEntityPointer = std::shared_ptr<T>;

    virtual void onAddToScene(const EntityItemPointer& entity) override final {
        Parent::onAddToScene(entity);
        onAddToSceneTyped(_typedEntity);
    }

    virtual void onRemoveFromScene(const EntityItemPointer& entity) override final {
        Parent::onRemoveFromScene(entity);
        onRemoveFromSceneTyped(_typedEntity);
    }

    using Parent::needsRenderUpdateFromEntity;
    // Returns true if the item in question needs to have updateInScene called because of changes in the entity
    virtual bool needsRenderUpdateFromEntity(const EntityItemPointer& entity) const override final {
        return Parent::needsRenderUpdateFromEntity(entity) || needsRenderUpdateFromTypedEntity(_typedEntity);
    }

    virtual void doRenderUpdateSynchronous(const ScenePointer& scene, Transaction& transaction, const EntityItemPointer& entity) override final {
        Parent::doRenderUpdateSynchronous(scene, transaction, entity);
        doRenderUpdateSynchronousTyped(scene, transaction, _typedEntity);
    }

    virtual void doRenderUpdateAsynchronous(const EntityItemPointer& entity) override final {
        Parent::doRenderUpdateAsynchronous(entity);
        doRenderUpdateAsynchronousTyped(_typedEntity);
    }

    virtual bool needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const { return false; }
    virtual void doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) { }
    virtual void doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) { }
    virtual void onAddToSceneTyped(const TypedEntityPointer& entity) { }
    virtual void onRemoveFromSceneTyped(const TypedEntityPointer& entity) { }

private:
    const TypedEntityPointer _typedEntity;
};

} } // namespace render::entities

#endif // hifi_RenderableEntityItem_h
