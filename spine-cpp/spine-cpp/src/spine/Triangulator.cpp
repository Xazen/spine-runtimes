/******************************************************************************
* Spine Runtimes Software License v2.5
*
* Copyright (c) 2013-2016, Esoteric Software
* All rights reserved.
*
* You are granted a perpetual, non-exclusive, non-sublicensable, and
* non-transferable license to use, install, execute, and perform the Spine
* Runtimes software and derivative works solely for personal or internal
* use. Without the written permission of Esoteric Software (see Section 2 of
* the Spine Software License Agreement), you may not (a) modify, translate,
* adapt, or develop new applications using the Spine Runtimes or otherwise
* create derivative works or improvements of the Spine Runtimes or (b) remove,
* delete, alter, or obscure any trademarks or any copyright, trademark, patent,
* or other intellectual property or proprietary rights notices on or in the
* Software, including any copy thereof. Redistributions in binary or source
* form must include this license and terms.
*
* THIS SOFTWARE IS PROVIDED BY ESOTERIC SOFTWARE "AS IS" AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
* EVENT SHALL ESOTERIC SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, BUSINESS INTERRUPTION, OR LOSS OF
* USE, DATA, OR PROFITS) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
* IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <spine/Triangulator.h>

#include <spine/MathUtil.h>

namespace Spine
{
    Vector<int> Triangulator::triangulate(Vector<float>& vertices)
    {
        int vertexCount = static_cast<int>(vertices.size() >> 1);
        
        Vector<int>& indices = _indices;
        indices.clear();
        indices.reserve(vertexCount);
        for (int i = 0; i < vertexCount; ++i)
        {
            indices[i] = i;
        }
        
        Vector<bool>& isConcaveArray = _isConcaveArray;
        isConcaveArray.reserve(vertexCount);
        for (int i = 0, n = vertexCount; i < n; ++i)
        {
            isConcaveArray[i] = isConcave(i, vertexCount, vertices, indices);
        }
        
        Vector<int>& triangles = _triangles;
        triangles.clear();
        triangles.reserve(MAX(0, vertexCount - 2) << 2);
        
        while (vertexCount > 3)
        {
            // Find ear tip.
            int previous = vertexCount - 1, i = 0, next = 1;
            
            // outer:
            while (true)
            {
                if (!isConcaveArray[i])
                {
                    int p1 = indices[previous] << 1, p2 = indices[i] << 1, p3 = indices[next] << 1;
                    float p1x = vertices[p1], p1y = vertices[p1 + 1];
                    float p2x = vertices[p2], p2y = vertices[p2 + 1];
                    float p3x = vertices[p3], p3y = vertices[p3 + 1];
                    for (int ii = (next + 1) % vertexCount; ii != previous; ii = (ii + 1) % vertexCount)
                    {
                        if (!isConcaveArray[ii])
                        {
                            continue;
                        }
                        
                        int v = indices[ii] << 1;
                        float vx = vertices[v], vy = vertices[v + 1];
                        if (positiveArea(p3x, p3y, p1x, p1y, vx, vy))
                        {
                            if (positiveArea(p1x, p1y, p2x, p2y, vx, vy))
                            {
                                if (positiveArea(p2x, p2y, p3x, p3y, vx, vy))
                                {
                                    goto break_outer; // break outer;
                                }
                            }
                        }
                    }
                    break;
                }
            break_outer:
                
                if (next == 0)
                {
                    do
                    {
                        if (!isConcaveArray[i])
                        {
                            break;
                        }
                        i--;
                    } while (i > 0);
                    break;
                }
                
                previous = i;
                i = next;
                next = (next + 1) % vertexCount;
            }
            
            // Cut ear tip.
            triangles.push_back(indices[(vertexCount + i - 1) % vertexCount]);
            triangles.push_back(indices[i]);
            triangles.push_back(indices[(i + 1) % vertexCount]);
            indices.RemoveAt(i);
            isConcaveArray.RemoveAt(i);
            vertexCount--;
            
            int previousIndex = (vertexCount + i - 1) % vertexCount;
            int nextIndex = i == vertexCount ? 0 : i;
            isConcaveArray[previousIndex] = isConcave(previousIndex, vertexCount, vertices, indices);
            isConcaveArray[nextIndex] = isConcave(nextIndex, vertexCount, vertices, indices);
        }
        
        if (vertexCount == 3)
        {
            triangles.push_back(indices[2]);
            triangles.push_back(indices[0]);
            triangles.push_back(indices[1]);
        }
        
        return triangles;
    }
    
    Vector<Vector<float> > Triangulator::decompose(Vector<float>& vertices, Vector<int>& triangles)
    {
        Vector<Vector<float> >& convexPolygons = _convexPolygons;
        for (int i = 0, n = convexPolygons.size(); i < n; ++i)
        {
            polygonPool.Free(convexPolygons[i]);
        }
        convexPolygons.Clear();
        
        Vector<Vector<int> >& convexPolygonsIndices = _convexPolygonsIndices;
        for (int i = 0, n = convexPolygonsIndices.size(); i < n; ++i)
        {
            _polygonIndicesPool.free(convexPolygonsIndices[i]);
        }
        convexPolygonsIndices.clear();
        
        var polygonIndices = _polygonIndicesPool.Obtain();
        polygonIndices.Clear();
        
        var polygon = polygonPool.Obtain();
        polygon.Clear();
        
        // Merge subsequent triangles if they form a triangle fan.
        int fanBaseIndex = -1, lastwinding = 0;
        int[] trianglesItems = triangles.Items;
        for (int i = 0, n = triangles.Count; i < n; i += 3)
        {
            int t1 = trianglesItems[i] << 1, t2 = trianglesItems[i + 1] << 1, t3 = trianglesItems[i + 2] << 1;
            float x1 = vertices[t1], y1 = vertices[t1 + 1];
            float x2 = vertices[t2], y2 = vertices[t2 + 1];
            float x3 = vertices[t3], y3 = vertices[t3 + 1];
            
            // If the base of the last triangle is the same as this triangle, check if they form a convex polygon (triangle fan).
            var merged = false;
            if (fanBaseIndex == t1)
            {
                int o = polygon.Count - 4;
                float[] p = polygon.Items;
                int winding1 = winding(p[o], p[o + 1], p[o + 2], p[o + 3], x3, y3);
                int winding2 = winding(x3, y3, p[0], p[1], p[2], p[3]);
                if (winding1 == lastwinding && winding2 == lastwinding)
                {
                    polygon.Add(x3);
                    polygon.Add(y3);
                    polygonIndices.Add(t3);
                    merged = true;
                }
            }
            
            // Otherwise make this triangle the new base.
            if (!merged)
            {
                if (polygon.Count > 0)
                {
                    convexPolygons.Add(polygon);
                    convexPolygonsIndices.Add(polygonIndices);
                }
                else
                {
                    polygonPool.Free(polygon);
                    _polygonIndicesPool.Free(polygonIndices);
                }
                
                polygon = polygonPool.Obtain();
                polygon.Clear();
                polygon.Add(x1);
                polygon.Add(y1);
                polygon.Add(x2);
                polygon.Add(y2);
                polygon.Add(x3);
                polygon.Add(y3);
                polygonIndices = _polygonIndicesPool.obtain();
                polygonIndices.Clear();
                polygonIndices.Add(t1);
                polygonIndices.Add(t2);
                polygonIndices.Add(t3);
                lastwinding = winding(x1, y1, x2, y2, x3, y3);
                fanBaseIndex = t1;
            }
        }
        
        if (polygon.Count > 0)
        {
            convexPolygons.Add(polygon);
            convexPolygonsIndices.Add(polygonIndices);
        }
        
        // Go through the list of polygons and try to merge the remaining triangles with the found triangle fans.
        for (int i = 0, n = convexPolygons.Count; i < n; ++i)
        {
            polygonIndices = convexPolygonsIndices.Items[i];
            if (polygonIndices.Count == 0) continue;
            int firstIndex = polygonIndices.Items[0];
            int lastIndex = polygonIndices.Items[polygonIndices.Count - 1];
            
            polygon = convexPolygons.Items[i];
            int o = polygon.Count - 4;
            float[] p = polygon.Items;
            float prevPrevX = p[o], prevPrevY = p[o + 1];
            float prevX = p[o + 2], prevY = p[o + 3];
            float firstX = p[0], firstY = p[1];
            float secondX = p[2], secondY = p[3];
            int winding = winding(prevPrevX, prevPrevY, prevX, prevY, firstX, firstY);
            
            for (int ii = 0; ii < n; ++ii)
            {
                if (ii == i)
                {
                    continue;
                }
                
                var otherIndices = convexPolygonsIndices.Items[ii];
                
                if (otherIndices.Count != 3)
                {
                    continue;
                }
                
                int otherFirstIndex = otherIndices.Items[0];
                int otherSecondIndex = otherIndices.Items[1];
                int otherLastIndex = otherIndices.Items[2];
                
                var otherPoly = convexPolygons.Items[ii];
                float x3 = otherPoly.Items[otherPoly.Count - 2], y3 = otherPoly.Items[otherPoly.Count - 1];
                
                if (otherFirstIndex != firstIndex || otherSecondIndex != lastIndex)
                {
                    continue;
                }
                
                int winding1 = winding(prevPrevX, prevPrevY, prevX, prevY, x3, y3);
                int winding2 = winding(x3, y3, firstX, firstY, secondX, secondY);
                if (winding1 == winding && winding2 == winding)
                {
                    otherPoly.Clear();
                    otherIndices.Clear();
                    polygon.Add(x3);
                    polygon.Add(y3);
                    polygonIndices.Add(otherLastIndex);
                    prevPrevX = prevX;
                    prevPrevY = prevY;
                    prevX = x3;
                    prevY = y3;
                    ii = 0;
                }
            }
        }
        
        // Remove empty polygons that resulted from the merge step above.
        for (int i = convexPolygons.Count - 1; i >= 0; --i)
        {
            polygon = convexPolygons.Items[i];
            if (polygon.Count == 0)
            {
                convexPolygons.RemoveAt(i);
                polygonPool.Free(polygon);
                polygonIndices = convexPolygonsIndices.Items[i];
                convexPolygonsIndices.RemoveAt(i);
                _polygonIndicesPool.Free(polygonIndices);
            }
        }
        
        return convexPolygons;
    }
    
    bool Triangulator::isConcave(int index, int vertexCount, Vector<float>& vertices, Vector<int>& indices)
    {
        int previous = indices[(vertexCount + index - 1) % vertexCount] << 1;
        int current = indices[index] << 1;
        int next = indices[(index + 1) % vertexCount] << 1;
        
        return !positiveArea(vertices[previous], vertices[previous + 1], vertices[current], vertices[current + 1], vertices[next],
                             vertices[next + 1]);
    }
    
    bool Triangulator::positiveArea(float p1x, float p1y, float p2x, float p2y, float p3x, float p3y)
    {
        return p1x * (p3y - p2y) + p2x * (p1y - p3y) + p3x * (p2y - p1y) >= 0;
    }
    
    int Triangulator::winding(float p1x, float p1y, float p2x, float p2y, float p3x, float p3y)
    {
        float px = p2x - p1x, py = p2y - p1y;
        
        return p3x * py - p3y * px + px * p1y - p1x * py >= 0 ? 1 : -1;
    }
}
