/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Options.h>
#include <vsg/io/stream.h>
#include <vsg/nodes/Transform.h>
#include <vsg/utils/PolytopeIntersector.h>

#include <iostream>

using namespace vsg;

namespace vsg
{

std::ostream& operator<<(std::ostream& output, const vsg::Polytope& polytope)
{
    output<<"Polytope "<<&polytope<<" {"<<std::endl;
    for(auto& pl : polytope)
    {
        output<<"   "<<pl<<std::endl;
    }
    output<<"}"<<std::endl;
    return output;
}

}

template<typename V>
struct TriangleIntersector
{
    using value_type = V;
    using vec_type = t_vec3<value_type>;

    dvec3 start;
    dvec3 end;
    uint32_t instanceIndex = 0;

    vec_type _d;
    value_type _length;
    value_type _inverse_length;

    vec_type _d_invX;
    vec_type _d_invY;
    vec_type _d_invZ;

    PolytopeIntersector& intersector;
    ref_ptr<const vec3Array> vertices;

    TriangleIntersector(PolytopeIntersector& in_intersector, const dvec3& in_start, const dvec3& in_end, ref_ptr<const vec3Array> in_vertices) :
        start(in_start),
        end(in_end),
        intersector(in_intersector),
        vertices(in_vertices)
    {

        _d = end - start;
        _length = length(_d);
        _inverse_length = (_length != 0.0) ? 1.0 / _length : 0.0;
        _d *= _inverse_length;

        _d_invX = _d.x != 0.0 ? _d / _d.x : vec_type(0.0, 0.0, 0.0);
        _d_invY = _d.y != 0.0 ? _d / _d.y : vec_type(0.0, 0.0, 0.0);
        _d_invZ = _d.z != 0.0 ? _d / _d.z : vec_type(0.0, 0.0, 0.0);
    }

    /// intersect with a single triangle
    bool intersect(uint32_t i0, uint32_t i1, uint32_t i2)
    {
        const vec3& v0 = vertices->at(i0);
        const vec3& v1 = vertices->at(i1);
        const vec3& v2 = vertices->at(i2);

        vec_type T = vec_type(start) - vec_type(v0);
        vec_type E2 = vec_type(v2) - vec_type(v0);
        vec_type E1 = vec_type(v1) - vec_type(v0);

        vec_type P = cross(_d, E2);

        value_type det = dot(P, E1);

        value_type r, r0, r1, r2;

        const value_type epsilon = 1e-10;
        if (det > epsilon)
        {
            value_type u = dot(P, T);
            if (u < 0.0 || u > det) return false;

            vec_type Q = cross(T, E1);
            value_type v = dot(Q, _d);
            if (v < 0.0 || v > det) return false;

            if ((u + v) > det) return false;

            value_type inv_det = 1.0 / det;
            value_type t = dot(Q, E2) * inv_det;
            if (t < 0.0 || t > _length) return false;

            u *= inv_det;
            v *= inv_det;

            r0 = 1.0 - u - v;
            r1 = u;
            r2 = v;
            r = t * _inverse_length;
        }
        else if (det < -epsilon)
        {
            value_type u = dot(P, T);
            if (u > 0.0 || u < det) return false;

            vec_type Q = cross(T, E1);
            value_type v = dot(Q, _d);
            if (v > 0.0 || v < det) return false;

            if ((u + v) < det) return false;

            value_type inv_det = 1.0 / det;
            value_type t = dot(Q, E2) * inv_det;
            if (t < 0.0 || t > _length) return false;

            u *= inv_det;
            v *= inv_det;

            r0 = 1.0 - u - v;
            r1 = u;
            r2 = v;
            r = t * _inverse_length;
        }
        else
        {
            return false;
        }

        dvec3 intersection = dvec3(dvec3(v0) * double(r0) + dvec3(v1) * double(r1) + dvec3(v2) * double(r2));
        intersector.add(intersection, double(r), {{i0, r0}, {i1, r1}, {i2, r2}}, instanceIndex);

        return true;
    }
};

PolytopeIntersector::PolytopeIntersector(const Polytope& in_polytope, ref_ptr<ArrayState> initialArrayData) :
    Inherit(initialArrayData)
{
    _polytopeStack.push_back(in_polytope);
}

PolytopeIntersector::PolytopeIntersector(const Camera& camera, double xMin, double yMin, double xMax, double yMax, ref_ptr<ArrayState> initialArrayData) :
    Inherit(initialArrayData)
{
    auto viewport = camera.getViewport();

    info("\nPolytopeIntersector::PolytopeIntersector(camera, ", xMin, ", ", yMin, ", ", xMax, ", ", yMax, ")");

    double ndc_xMin = (viewport.width > 0) ? (2.0 * (xMin - static_cast<double>(viewport.x)) / static_cast<double>(viewport.width) - 1.0) : xMin;
    double ndc_xMax = (viewport.width > 0) ? (2.0 * (xMax - static_cast<double>(viewport.x)) / static_cast<double>(viewport.width) - 1.0) : xMax;

    double ndc_yMin = (viewport.height > 0) ? (1.0 - 2.0 * (yMax - static_cast<double>(viewport.y)) / static_cast<double>(viewport.height)) : yMin;
    double ndc_yMax = (viewport.height > 0) ? (1.0 - 2.0 * (yMin - static_cast<double>(viewport.y)) / static_cast<double>(viewport.height)) : yMax;

    info("ndc_xMin ", ndc_xMin);
    info("ndc_xMax ", ndc_xMax);
    info("ndc_yMin ", ndc_yMin);
    info("ndc_yMax ", ndc_yMax);

    auto projectionMatrix = camera.projectionMatrix->transform();
    auto inverse_projectionMatrix = camera.projectionMatrix->inverse();
    auto viewMatrix = camera.viewMatrix->transform();
    auto inverse_viewMatrix = camera.viewMatrix->inverse();

    bool reverse_depth = (projectionMatrix(2, 2) > 0.0);

    vsg::Polytope clipspace;
    clipspace.push_back(dplane(1.0, 0.0, 0.0, -ndc_xMin));
    clipspace.push_back(dplane(-1.0, 0.0, 0.0, ndc_xMax));
    clipspace.push_back(dplane(0.0, 1.0, 0.0, -ndc_yMin));
    clipspace.push_back(dplane(0.0, -1.0, 0.0, ndc_yMax));

    if (reverse_depth)
    {
        clipspace.push_back(dplane(0.0, 0.0, 1.0, -viewport.maxDepth));
        clipspace.push_back(dplane(0.0, 0.0, -1.0, viewport.minDepth));
    }
    else
    {
        clipspace.push_back(dplane(0.0, 0.0, -1.0, viewport.maxDepth));
        clipspace.push_back(dplane(0.0, 0.0, 1.0, -viewport.minDepth));
    }

    vsg::Polytope eyespace;
    for(auto& pl : clipspace)
    {
        eyespace.push_back(pl * projectionMatrix);
    }

    vsg::Polytope worldspace;
    for(auto& pl : eyespace)
    {
        worldspace.push_back(pl * viewMatrix);
    }

    _polytopeStack.push_back(worldspace);

    std::cout<<"Clip space : "<<clipspace<<std::endl;
    std::cout<<"Eye space : "<<eyespace<<std::endl;
    std::cout<<"World space : "<<worldspace<<std::endl;

}

PolytopeIntersector::Intersection::Intersection(const dvec3& in_localIntersection, const dvec3& in_worldIntersection, double in_ratio, const dmat4& in_localToWorld, const NodePath& in_nodePath, const DataList& in_arrays, const IndexRatios& in_indexRatios, uint32_t in_instanceIndex) :
    localIntersection(in_localIntersection),
    worldIntersection(in_worldIntersection),
    ratio(in_ratio),
    localToWorld(in_localToWorld),
    nodePath(in_nodePath),
    arrays(in_arrays),
    indexRatios(in_indexRatios),
    instanceIndex(in_instanceIndex)
{
}

ref_ptr<PolytopeIntersector::Intersection> PolytopeIntersector::add(const dvec3& coord, double ratio, const IndexRatios& indexRatios, uint32_t instanceIndex)
{
    ref_ptr<Intersection> intersection;

    auto localToWorld = computeTransform(_nodePath);
    intersection = Intersection::create(coord, localToWorld * coord, ratio, localToWorld, _nodePath, arrayStateStack.back()->arrays, indexRatios, instanceIndex);
    intersections.emplace_back(intersection);

    return intersection;
}

void PolytopeIntersector::pushTransform(const Transform& transform)
{
    vsg::info("PolytopeIntersector::pushTransform(", transform.className(), ")");


    auto& l2wStack = localToWorldStack();
    auto& w2lStack = worldToLocalStack();

    dmat4 localToWorld = l2wStack.empty() ? transform.transform(dmat4{}) : transform.transform(l2wStack.back());
    dmat4 worldToLocal = inverse(localToWorld);

    l2wStack.push_back(localToWorld);
    w2lStack.push_back(worldToLocal);

    const auto& worldspace = _polytopeStack.front();

    Polytope localspace;
    for(auto& pl : worldspace)
    {
        localspace.push_back(pl * localToWorld);
    }

    _polytopeStack.push_back(localspace);

}

void PolytopeIntersector::popTransform()
{
    vsg::info("PolytopeIntersector::popTransform()");

    _polytopeStack.pop_back();
    localToWorldStack().pop_back();
    worldToLocalStack().pop_back();
}

bool PolytopeIntersector::intersects(const dsphere& bs)
{
    //debug("intersects( center = ", bs.center, ", radius = ", bs.radius, ")");
    if (!bs.valid()) return false;


#if 0
    const dvec3& start = lineSegment.start;
    const dvec3& end = lineSegment.end;

    dvec3 sm = start - bs.center;
    double c = length2(sm) - bs.radius * bs.radius;
    if (c < 0.0) return true;

    dvec3 se = end - start;
    double a = length2(se);
    double b = dot(sm, se) * 2.0;
    double d = b * b - 4.0 * a * c;

    if (d < 0.0) return false;

    d = sqrt(d);

    double div = 1.0 / (2.0 * a);

    double r1 = (-b - d) * div;
    double r2 = (-b + d) * div;

    if (r1 <= 0.0 && r2 <= 0.0) return false;
    if (r1 >= 1.0 && r2 >= 1.0) return false;
#else
    info("PolytopeIntersector::intersects(const dsphere& bs) todo.");
#endif
    // passed all the rejection tests so line must intersect bounding sphere, return true.
    return true;
}

bool PolytopeIntersector::intersectDraw(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount)
{
    size_t previous_size = intersections.size();
#if 0
    auto& arrayState = *arrayStateStack.back();
    if (arrayState.topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || vertexCount < 3) return false;

    const auto& ls = _lineSegmentStack.back();

    uint32_t lastIndex = instanceCount > 1 ? (firstInstance + instanceCount) : firstInstance + 1;
    for (uint32_t instanceIndex = firstInstance; instanceIndex < lastIndex; ++instanceIndex)
    {
        TriangleIntersector<double> triIntersector(*this, ls.start, ls.end, arrayState.vertexArray(instanceIndex));
        if (!triIntersector.vertices) return false;

        uint32_t endVertex = int((firstVertex + vertexCount) / 3.0f) * 3;

        for (uint32_t i = firstVertex; i < endVertex; i += 3)
        {
            triIntersector.intersect(i, i + 1, i + 2);
        }
    }
#else
    info("PolytopeIntersector::intersectDraw(", firstVertex, ", ", vertexCount, ", ", firstInstance, ", ", instanceCount,")) todo.");
#endif
    return intersections.size() != previous_size;
}

bool PolytopeIntersector::intersectDrawIndexed(uint32_t firstIndex, uint32_t indexCount, uint32_t firstInstance, uint32_t instanceCount)
{
    size_t previous_size = intersections.size();
#if 0
    auto& arrayState = *arrayStateStack.back();
    if (arrayState.topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || indexCount < 3) return false;
    const auto& ls = _lineSegmentStack.back();

    uint32_t lastIndex = instanceCount > 1 ? (firstInstance + instanceCount) : firstInstance + 1;
    for (uint32_t instanceIndex = firstInstance; instanceIndex < lastIndex; ++instanceIndex)
    {
        TriangleIntersector<double> triIntersector(*this, ls.start, ls.end, arrayState.vertexArray(instanceIndex));
        if (!triIntersector.vertices) continue;

        triIntersector.instanceIndex = instanceIndex;

        uint32_t endIndex = int((firstIndex + indexCount) / 3.0f) * 3;

        if (ushort_indices)
        {
            for (uint32_t i = firstIndex; i < endIndex; i += 3)
            {
                triIntersector.intersect(ushort_indices->at(i), ushort_indices->at(i + 1), ushort_indices->at(i + 2));
            }
        }
        else if (uint_indices)
        {
            for (uint32_t i = firstIndex; i < endIndex; i += 3)
            {
                triIntersector.intersect(uint_indices->at(i), uint_indices->at(i + 1), uint_indices->at(i + 2));
            }
        }
    }
#else
    info("PolytopeIntersector::intersectDrawIndexed(", firstIndex, ", ", indexCount, ", ", firstInstance, ", ", instanceCount,")) todo.");
#endif

    return intersections.size() != previous_size;
}