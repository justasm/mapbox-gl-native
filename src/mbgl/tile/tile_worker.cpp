#include <mbgl/text/collision_tile.hpp>
#include <mbgl/tile/tile_worker.hpp>
#include <mbgl/tile/geometry_tile.hpp>
#include <mbgl/style/style_layer.hpp>
#include <mbgl/style/style_bucket_parameters.hpp>
#include <mbgl/layer/background_layer.hpp>
#include <mbgl/layer/custom_layer.hpp>
#include <mbgl/layer/symbol_layer.hpp>
#include <mbgl/sprite/sprite_atlas.hpp>
#include <mbgl/geometry/glyph_atlas.hpp>
#include <mbgl/renderer/symbol_bucket.hpp>
#include <mbgl/platform/log.hpp>
#include <mbgl/util/constants.hpp>
#include <mbgl/util/string.hpp>
#include <mbgl/util/exception.hpp>
#include <utility>

using namespace mbgl;

TileWorker::TileWorker(const OverscaledTileID& id_,
                       std::string sourceID_,
                       SpriteStore& spriteStore_,
                       GlyphAtlas& glyphAtlas_,
                       GlyphStore& glyphStore_,
                       const std::atomic<bool>& obsolete_,
                       const MapMode mode_)
    : id(id_),
      sourceID(std::move(sourceID_)),
      spriteStore(spriteStore_),
      glyphAtlas(glyphAtlas_),
      glyphStore(glyphStore_),
      obsolete(obsolete_),
      mode(mode_) {
}

TileWorker::~TileWorker() {
    glyphAtlas.removeGlyphs(reinterpret_cast<uintptr_t>(this));
}

TileParseResult TileWorker::parseAllLayers(std::vector<std::unique_ptr<StyleLayer>> layers_,
                                           std::unique_ptr<const GeometryTile> geometryTile_,
                                           PlacementConfig config) {
    // We're doing a fresh parse of the tile, because the underlying data has changed.
    pending.clear();
    placementPending.clear();
    partialParse = false;
    featureIndex = std::make_unique<FeatureIndex>();
    geometryTile = std::move(geometryTile_);

    // Store the layers for use in redoPlacement.
    layers = std::move(layers_);

    // We're storing a set of bucket names we've parsed to avoid parsing a bucket twice that is
    // referenced from more than one layer
    std::set<std::string> parsed;

    for (auto i = layers.rbegin(); i != layers.rend(); i++) {
        const StyleLayer* layer = i->get();
        if (parsed.find(layer->bucketName()) == parsed.end()) {
            parsed.emplace(layer->bucketName());
            parseLayer(layer);
        }
        featureIndex->addBucketLayerName(layer->bucketName(), layer->id);
    }

    return prepareResult(config);
}

TileParseResult TileWorker::parsePendingLayers(const PlacementConfig config) {
    // Try parsing the remaining layers that we couldn't parse in the first step due to missing
    // dependencies.
    for (auto it = pending.begin(); it != pending.end();) {
        auto& layer = *it->first;
        auto bucket = dynamic_cast<SymbolBucket*>(it->second.get());
        assert(bucket); // Only symbol layers can be pending, so the dynamic cast should never fail.

        if (!bucket->needsDependencies(glyphStore, spriteStore)) {
            bucket->addFeatures(reinterpret_cast<uintptr_t>(this),
                                *layer.spriteAtlas,
                                glyphAtlas,
                                glyphStore);
            placementPending.emplace(layer.bucketName(), std::move(it->second));
            pending.erase(it++);
            continue;
        }

        // Advance the iterator here; we're skipping this when erasing an element from this list.
        ++it;
    }

    return prepareResult(config);
}

TileParseResult TileWorker::prepareResult(const PlacementConfig& config) {
    result.complete = pending.empty();

    if (result.complete) {
        featureIndex->setCollisionTile(placeLayers(config));
        result.featureIndex = std::move(featureIndex);
        result.geometryTile = std::move(geometryTile);
    }

    return std::move(result);
}

std::unique_ptr<CollisionTile> TileWorker::placeLayers(const PlacementConfig config) {
    auto collisionTile = redoPlacement(&placementPending, config);
    for (auto &p : placementPending) {
        p.second->swapRenderData();
        insertBucket(p.first, std::move(p.second));
    }
    placementPending.clear();
    return collisionTile;
}

std::unique_ptr<CollisionTile> TileWorker::redoPlacement(
    const std::unordered_map<std::string, std::unique_ptr<Bucket>>* buckets,
    PlacementConfig config) {

    auto collisionTile = std::make_unique<CollisionTile>(config);

    for (auto i = layers.rbegin(); i != layers.rend(); i++) {
        const auto it = buckets->find((*i)->id);
        if (it != buckets->end()) {
            it->second->placeFeatures(*collisionTile);
        }
    }

    return collisionTile;
}

void TileWorker::parseLayer(const StyleLayer* layer) {
    // Cancel early when parsing.
    if (obsolete)
        return;

    // Background and custom layers are special cases.
    if (layer->is<BackgroundLayer>() || layer->is<CustomLayer>())
        return;

    // Skip this bucket if we are to not render this
    if ((layer->source != sourceID) ||
        (id.overscaledZ < std::floor(layer->minZoom)) ||
        (id.overscaledZ >= std::ceil(layer->maxZoom)) ||
        (layer->visibility == VisibilityType::None)) {
        return;
    }

    auto geometryLayer = geometryTile->getLayer(layer->sourceLayer);
    if (!geometryLayer) {
        // The layer specified in the bucket does not exist. Do nothing.
        if (debug::tileParseWarnings) {
            Log::Warning(Event::ParseTile, "layer '%s' does not exist in tile %s",
                    layer->sourceLayer.c_str(), util::toString(id).c_str());
        }
        return;
    }

    StyleBucketParameters parameters(id,
                                     *geometryLayer,
                                     obsolete,
                                     reinterpret_cast<uintptr_t>(this),
                                     partialParse,
                                     spriteStore,
                                     glyphAtlas,
                                     glyphStore,
                                     *featureIndex,
                                     mode);

    std::unique_ptr<Bucket> bucket = layer->createBucket(parameters);

    if (layer->is<SymbolLayer>()) {
        if (partialParse) {
            // We cannot parse this bucket yet. Instead, we're saving it for later.
            pending.emplace_back(layer->as<SymbolLayer>(), std::move(bucket));
        } else {
            placementPending.emplace(layer->bucketName(), std::move(bucket));
        }
    } else {
        insertBucket(layer->bucketName(), std::move(bucket));
    }
}

void TileWorker::insertBucket(const std::string& name, std::unique_ptr<Bucket> bucket) {
    if (bucket->hasData()) {
        result.buckets.emplace(name, std::move(bucket));
    }
}
