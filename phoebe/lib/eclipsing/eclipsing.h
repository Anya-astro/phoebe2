#if !defined(__eclipsing_h)
#define __eclipsing_h

/*
  Library for determining the visibility (aka eclipsing) of triangulated 
  surfaces describing the nboundary of 3D objects.
  
  The algorithms are build on concepts behind the hidden surface removal or
  determining visibility of 3d objects.
  
  
  Ref: https://en.wikipedia.org/wiki/Hidden_surface_determination
  
  Author: Martin Horvat, May 2016
*/

#include <iostream>
#include <cmath>
#include <vector>
#include <set>
#include <ctime>

#include "../triang/triang_marching.h"
#include "../utils/utils.h"
#include "clipper.h"

//#include "clipper_orig.h"

/*
  Check if point p is strictly inside a triangle defined by vertices 
  {v1,v2,v2}

  Input:
    p - tested point
    v1, v2, v3 - vertices of a triangle
    
  Return:
    true if the point is striclty inside, false otherwise
  
*/
template <class T>
bool point_in_triangle(T p[2], T v1[2], T v2[2], T v3[2], T bb[4]){
  
  // First check if the point is inside the binding box of the triangle.
  if (bb[0] < p[0] && p[0] < bb[1] && bb[2] < p[1] && p[1] < bb[3]) {

    // If it's not, then check if it's inside the triangle.
    
    bool b1 = (p[0]- v1[0])*(v2[1] - v1[1]) > (p[1]- v1[1])*(v2[0] - v1[0]), // sign((p-v1)x(v2-v1)).k  
         b2 = (p[0]- v2[0])*(v3[1] - v2[1]) > (p[1]- v2[1])*(v3[0] - v2[0]); // sign((p-v2)x(v3-v2).k 
        
    if (b1 == b2) {
      b2 = (p[0]- v3[0])*(v1[1] - v3[1]) > (p[1]- v3[1])*(v1[0] - v3[0]);  // sign((p-v3)x(v1-v3).k 
      if (b1 == b2) return true;
    }
  }
  return false;
}
/*
  Check if point p is strictly inside a triangle defined by vertices 
  {v1,v2,v2}

  Input:
    p - tested point
    v[3][2] = {v1, v2, v3} - vertices of a triangle
    
  Return:
    true if the point is striclty inside, false otherwise
  
*/

template <class T>
bool point_in_triangle(T p[2], T v[3][2], T bb[4]){
  
  // First check if the point is inside the binding box of the triangle.
  if (bb[0] < p[0] && p[0] < bb[1] && bb[2] < p[1] && p[1] < bb[3]) {

    // If it's not, then check if it's inside the triangle
    
    bool b1 = (p[0]- v[0][0])*(v[1][1] - v[0][1]) > (p[1]- v[0][1])*(v[1][0] - v[0][0]), // sign((p-v1)x(v2-v1)).k  
         b2 = (p[0]- v[1][0])*(v[2][1] - v[1][1]) > (p[1]- v[1][1])*(v[2][0] - v[1][0]); // sign((p-v2)x(v3-v2).k 
        
    if (b1 == b2) {
      b2 = (p[0]- v[2][0])*(v[0][1] - v[2][1]) > (p[1]- v[2][1])*(v[0][0] - v[2][0]);  // sign((p-v3)x(v1-v3).k 
      if (b1 == b2) return true;
    }
  }
  
  return false;
}

/*
  Check if the boundary boxes {minX,maxX,minY,maxY} of two triangles stricty 
  overlap.

  Input:
    bA - boundary box of A triangle 
    bB - boundary box of B triangle

  Return:
    true if there is an overlap, false otherwise
  
*/

template <class T> 
bool bb_overlap(T bA[4], T bB[4]) {
  
  // if (A.X1 < B.X2 && A.X2 > B.X1 && 
  //     A.Y1 < B.Y2 && A.Y2 > B.Y1) 
    
  if (bA[0] < bB[1] && bA[1] > bB[0] && 
      bA[2] < bB[3] && bA[3] > bB[2] ) return true;
      
  return false;
}


/*
  Check if the boundary boxes {minX,maxX,minY,maxY} of two triangles stricty 
  overlap.

  Input:
    b = {bA,bB} - boundary box of A triangle and B triangle

  Return:
    true if there is an overlap, false otherwise
  
*/

template <class T> 
inline bool bb_overlap(T b[2][4]) {
  
  // if (A.X1 < B.X2 && A.X2 > B.X1 && 
  //       A.Y1 < B.Y2 && A.Y2 > B.Y1) 
    
  if (b[0][0] < b[1][1] && b[0][1] > b[1][0] && 
      b[0][2] < b[1][3] && b[0][3] > b[1][2] ) return true;
      
  return false;
}


/*
  Rough types of visibilities
*/
enum Tvisibility {
  hidden, 
  partially_hidden, 
  visible
};

/*
  Determine rough visibility of triangles in a triangulated surfaces in direction v. 
  The surface can be a union of closed surfaces.  The algorithm is the sequence of

    Back-face culling: 
      throwing away all surfaces which n_i v_i < 0
      only working for closed surfaces
    
    Painter's algorithm:
      depth ordering of triangles w.r.t. max_i {r_i v_i} 
      
    Pieter Degroote's approach of determining the type based on rays from vertices towards the observer passing through triangles afront.
    Check how many vertices of individual triangles are eclipsed:
      nr = 0 : visible
           1,2 : partially visible
           3   : hidden
    Used classification: a discussed triangle
      * is hidden if all of its vertices are obstructed
      * is partially visible if nr. of  obstructed of vertices < 3 or 
        if triangles in front it have vertices its projected image
    
    Comment:
    This algorithm is generally not applicable, but could work for 
    not too vivid structures and triangles of almost same 
    shapes and sizes, which are generated by maching triagulation algorithm.
  
  Input:
    view[3] - direction of the observer
    
    V - vector of vertices used in triangles
    T - vector of triangles defined by indices vertices
    N - vector of normals of triangles (to speed up repeated use)

  Output:
    M - vector of the visibility class of triangles
*/

template <class T>
void triangle_mesh_rough_visibility(
  double view[3], 
  std::vector<T3Dpoint<T> > & V,  
  std::vector<Ttriangle> & Tr,
  std::vector<T3Dpoint<T> > & N,
  std::vector<Tvisibility> & M
  ) {
  
  //
  // Defining the on-screen vector basis (t1,t2,view)
  //
  
  T b[3][3];
  
  create_basis(view, b);
  
  //
  // Back-face culling and storing all potentially visible triangles
  //
  
  int 
    Nt = Tr.size(), 
    Nv = V.size();
  
  // Sequence vertices in the on-screen basis
  // Vs = (x_0, y_0, z_0, x_1, y_1, z_1, ..., x_{Nv-1}, y_{Nv-1}, z_{Nv-1})
  T *Vs = new T [3*Nv];
  
  // If on-screen coordinates have been calculated
  // if ith vertex was projected onto screen bases Vst[i] = true
  bool *Vst = new bool [Nv];   
  for (int i = 0; i < Nv; ++i) Vst[i] = false;
  
  // prepare M for Nt data
  M.clear(); 
  M.resize(Nt, hidden); // initially all triangles are hidden
 
  // Prepare triangle to be sorted according to depth 
  struct Tt {
    
    int index;         // triangle index
         
    T z;               // maximal depth of the triangle
      
    Tt(){}
    
    Tt(const struct Tt & t): index(t.index), z(t.z) {}
    
    Tt(const int& index, const T &z):index(index), z(z) {}
    
    bool operator < (const Tt & rhs) const { return z > rhs.z; }
  };
  
  std::vector<Tt> Tv;  // vector of potentially visible triangles
  
  {
    int k, *t;
    
    T *n, *v[3];
  
    for (int i = 0; i < Nt; ++i){
      
      n = N[i].data;  // normal of the ith triangle
      
      if (n[0]*view[0] + n[1]*view[1] + n[2]*view[2] > 0) {
        
        // indices of vectices used in the ith triangle
        t = Tr[i].indices;
        
        // calculate projection onto screen 
        for (int j = 0; j < 3; ++j) {
          
          v[j] = Vs + 3*(k = t[j]);
          
          if (!Vst[k]) {  // projecting on the screens 
            trans_basis(V[k].data, v[j], b); 
            Vst[k] = true;
          }
        }
        
        Tv.emplace_back(i, utils::max3(v[0][2], v[1][2], v[2][2]));
        
        M[i] = visible;
      }
    }
  }
  
  //
  // Sort potentially visible triangles w.r.t. to max distance 
  // in direction of observation 
  //
  
  //clock_t  start = clock();
  std::sort(Tv.begin(), Tv.end());
  //std::cout << "sort=" << clock()-start << '\n';
  
  //
  // Calculate bounding boxes in (t1,t2) plane for all visible boxes
  //
  
  int Ntv = Tv.size();    // number of potentially visible triangles
  
  T *bb = new T [4*Ntv];  // seq. [minX1, maxX1, minY1, maxY1, minX2, maxX2, minY2, maxY2, ...]
  
  {
    int *t;
    
    T *pb = bb, *p, v[3][2];
   
    for (auto && tr: Tv) {
      
      // indices of points of a triangle in the ith triangle
      t = Tr[tr.index].indices; 
      
      // create easy access to vertices
      for (int j = 0; j < 3; ++j) {
        p = Vs + 3*t[j];
        v[j][0] = p[0];
        v[j][1] = p[1];
      }
      
      // min max for t1 direction stored in pb[0], pb[1]
      utils::minmax3(v[0][0], v[1][0], v[2][0], pb);    
      pb += 2;
      
      // min max for t2 direction stored in pb[0], pb[1]
      utils::minmax3(v[0][1], v[1][1], v[2][1], pb);
      pb += 2;
    }
  }
  
  //
  // Pieter Degroote's algorithm based on intersection of rays from vertices
  //
  
  // Vs is reused to fallow obstruction of vertices
  // true means that it is no obstructed == visible
  
  //clock_t  start = clock();
  
  for (int i = 0; i < Nv; ++i) Vst[i] = true; 
    
  {
  
    int ii, jj, kk, *ti, *tj;
    
    T vi[3][2], vj[3][2], pb[2][4], 
      *pbi = pb[0], *pbj = pb[1], *p;
   
    bool st[3];
    
    for (int i = 1; i < Ntv; ++i) {
      
      //
      // data about ii-th triangle   
      //
      p = bb + 4*i;                   // local copy of bounding box
      for (int k = 0; k < 4; ++k) pbi[k] = p[k];
          
      ii = Tv[i].index;               // index of discussed triangle
      ti = Tr[ii].indices;            // indices of vertices 
      for (int k = 0; k < 3; ++k) {   // local copy of vertices
        p = Vs + 3*ti[k];  
        vi[k][0] = p[0];
        vi[k][1] = p[1];
      }
       
      // Loop over the triangle in front of ii-th triangle
      for (int j = 0; j < i; ++j) if (M[jj = Tv[j].index] != hidden) {
        
        
        p = bb + 4*j;                  // local copy of bounding box
        for (int k = 0; k < 4; ++k) pbj[k] = p[k];
        
        //
        // Check if there is some overlap of bounding box
        // 
         
        if (!bb_overlap(pb)) continue;
        
        //
        // data about jj-th triangle
        //
        tj =  Tr[jj].indices;          // indices of vertices
        for (int k = 0; k < 3; ++k) {  // local copy of vertices           
          p = Vs + 3*tj[k]; 
          vj[k][0] = p[0];
          vj[k][1] = p[1];
        }
          
        // check if vectices of ii-th triangle are obscured by a triangle 
        // in front of it. If can not be obscued if there are same as vertices 
        // of ii
        
        for (int k = 0; k < 3; ++k) {
      
          kk = ti[k]; // index of k-th point of ii-th triangle
      
          st[k] = Vst[kk];
      
          if (st[k] && kk != tj[0] && kk != tj[1] && kk != tj[2] && point_in_triangle(vi[k], vj, pbj))
            st[k] = Vst[kk] = false;
        }
        
        // if all vertices are not visible triangle is marked as hidden
        // if just some are hidden then it is marked as partially hidden
        if (!(st[0] || st[1] || st[2])) {
          M[ii] = hidden; 
          break;
        } else if (!(st[0] && st[1] && st[2]))
          M[ii] = partially_hidden;
            
        // check if some of vectices of jj-th triangle are strictly inside 
        // the projected picture of ii-th. it this is so, the triangle 
        // is marked as (at least) partially hidden
        
        if (M[ii] == visible) {
          for (int k = 0; k < 3; ++k) {
          
            kk = tj[k]; // index of k-th point of jj-th triangle
          
            if (kk != ti[0] && kk != ti[1] && kk != ti[2])
              if (point_in_triangle(vj[k], vi, pbi)) {
                M[ii] = partially_hidden;
                break;
              }
          }
        }
      }
    }
  }
  //std::cout << "algo=" << clock()-start << '\n';
  
  delete [] bb;
  delete [] Vs;
  delete [] Vst;
}



/*  
  Determine rough visibility of triangles in a triangulated surfaces in direction v. 
  The surface can be a union of closed surfaces.  The algorithm is the sequence of

    Back-face culling: 
      throwing away all surfaces which n_i v_i < 0
      only working for closed surfaces
    
    Painter's algorithm:
      depth ordering of triangles w.r.t. max_i {r_i v_i} 
      
    Pieter Degroote's approach of determining the type: Check how many 
    vertices of individual triangles are eclipsed:
      nr = 0 : visible
           1,2 : partially visible
           3   : hidden
    Used classification: a discussed triangle
      * is hidden if all of its vertices are obstructed
      * is partially visible if nr. of  obstructed of vertices < 3 or 
        if triangles in front it have vertices its projected image
    
    Comment:
    This algorithm is generally not applicable, but could work for 
    not too vivid structures and triangles of almost same 
    shapes and sizes, which are generated by maching triagulation algorithm. 
    This algorithm has O(n^2) complexity, where n = number of forward 
    facing triangles.
  
  Input:
    view[3] - direction of the observer
    
    V - vector of vertices used in triangles
    Tr - vector of triangles defined by indices vertices
    N - vector of normals of triangles (to speed up repeated use)

  Output:
    M - vector of the visibility class of triangles

  Comment:
    A more elegant version of the previous triangle_rough_visibility(), but slower.

*/

template <class T>
void triangle_mesh_rough_visibility_elegant(
  double view[3], 
  std::vector<T3Dpoint<T> > & V,
  std::vector<Ttriangle> & Tr,
  std::vector<T3Dpoint<T>> & N,
  std::vector<Tvisibility> & M
  ) {
  
  //
  // Defining the on-screen vector basis (t1,t2,view)
  //
  
  T b[3][3];
  
  create_basis(view, b);
  
  //
  // Back-face culling and storing all potentially visible triangles
  //
  
  int 
    Nt = Tr.size(), 
    Nv = V.size();
  
  // Sequence vertices in the on-screen basis
  // Vs = (x_0, y_0, z_0, x_1, y_1, z_1, ..., x_{Nv-1}, y_{Nv-1}, z_{Nv-1})
  T *Vs = new T [3*Nv];
  
  // If on-screen coordinates have been calculated
  // if ith vertex was projected onto screen bases Vst[i] = true
  bool *Vst = new bool [Nv];   
  for (int i = 0; i < Nv; ++i) Vst[i] = false;
   
  // structure descriping potentially visible triangle
  struct Tt { 
    
    Tvisibility m;
    
    int 
      index,            // triangle index
      indices[3];       // indices of vertices
       
    T z,                // maximal depth of the triangle
      v[3][2],          // vertices on the screen
      bb[4];            // bounding box
          
    bool operator < (const Tt & rhs) const { return z > rhs.z; }
    
    bool point_in(T p[2]) const {
     
      // First check if the point is inside the binding box of the triangle.
      if (bb[0] < p[0] && p[0] < bb[1] && bb[2] < p[1] && p[1] < bb[3]) {

        // If it's not, then check if it's inside the triangle
        
        bool 
        // sign((p-v1)x(v2-v1)).k  
        b1 = (p[0]- v[0][0])*(v[1][1] - v[0][1]) > (p[1]- v[0][1])*(v[1][0] - v[0][0]), 
         // sign((p-v2)x(v3-v2).k 
        b2 = (p[0]- v[1][0])*(v[2][1] - v[1][1]) > (p[1]- v[1][1])*(v[2][0] - v[1][0]);
            
        if (b1 == b2) {
           // sign((p-v3)x(v1-v3).k
          b2 = (p[0]- v[2][0])*(v[0][1] - v[2][1]) > (p[1]- v[2][1])*(v[0][0] - v[2][0]); 
          if (b1 == b2) return true;
        }
      } 
      return false;
    }

    bool bb_overlap(const Tt & t) const {

      // if (A.X1 < B.X2 && A.X2 > B.X1 &&
      //     A.Y1 < B.Y2 && A.Y2 > B.Y1) 
        
      if (bb[0] < t.bb[1] && bb[1] > t.bb[0] && 
          bb[2] < t.bb[3] && bb[3] > t.bb[2] ) return true;
          
      return false;
    }

    bool bb_overlap(Tt *t) const {

      // if (A.X1 < B.X2 && A.X2 > B.X1 &&
      //     A.Y1 < B.Y2 && A.Y2 > B.Y1) 
        
      if (bb[0] < t->bb[1] && bb[1] > t->bb[0] && 
          bb[2] < t->bb[3] && bb[3] > t->bb[2] ) return true;
          
      return false;
    }

    
    bool index_notin(const int &k) const {
      return k != indices[0] && k != indices[1] && k != indices[2];
    }
  };
  
  std::vector<Tt> Tv;  // vector of potentially visible triangles
      
  {
      
    int k;
    
    T *n, *v[3];
  
    Tt tr;
    
    for (int i = 0; i < Nt; ++i){
      
      n = N[i].data;  // normal of the ith triangle
      
      if (n[0]*view[0] + n[1]*view[1] + n[2]*view[2] > 0) {
        
        tr.index = i;
        
        // calculate projection onto screen 
        for (int j = 0; j < 3; ++j) {
          
          k = tr.indices[j] = Tr[i].indices[j];
          
          v[j] = Vs + 3*k;
          
          if (!Vst[k]) {  // projecting on the screens 
            trans_basis(V[k].data, v[j], b); 
            Vst[k] = true;
          }
          
          tr.v[j][0] = v[j][0];
          tr.v[j][1] = v[j][1];
        }
        
        //  Calculate bounding boxes in (t1,t2) plane for all visible boxes
        utils::minmax3(tr.v[0][0], tr.v[1][0], tr.v[2][0], tr.bb);    
        utils::minmax3(tr.v[0][1], tr.v[1][1], tr.v[2][1], tr.bb + 2);
        
        // Calculate depth
        tr.z = utils::max3(v[0][2], v[1][2], v[2][2]);
        
        tr.m = visible;             
        
        Tv.push_back(tr);
      }
    }
  }
  
  delete [] Vs;
  
  //
  // Sort potentially visible triangles w.r.t. to max distance 
  // in direction of observation 
  //
  
  std::sort(Tv.begin(), Tv.end());
  
  //
  // Pieter Degroote's algorithm
  //
  
  // Vs is reused to fallow obstruction of vertices
  // true means that it is no obstructed == visible
    
  for (int i = 0; i < Nv; ++i) Vst[i] = true; 
  
  //std::cout << Tv.size() << " " << Tr.size() << "\n";
  
  //clock_t start = clock();  
  #if 1
  {
    int kk;

    bool st[3];
    
    Tt *Tv_b = Tv.data(), *Tv_e = Tv_b + Tv.size(), *Ti = Tv_b, *Tj;
    
    while (++Ti < Tv_e)
      for (Tj = Tv_b; Tj < Ti; ++Tj)  // Loop over triangles in front of Ti
        if (Tj->m != hidden && Ti->bb_overlap(Tj)) { 
          
          // check if vectices of Ti-th triangle are obscured by a triangle 
          // in front of it. If can not be obscued if there are same as vertices 
          // of Ti
              
          for (int k = 0; k < 3; ++k)
            if ((st[k] = Vst[kk = Ti->indices[k]]) && 
                Tj->index_notin(kk) && Tj->point_in(Ti->v[k]))
              st[k] = Vst[kk] = false;
          
          // if all vertices are not visible triangle is marked as hidden
          // if just some are hidden then it is marked as partially hidden
          if (!(st[0] || st[1] || st[2])) {
            Ti->m = hidden; 
            break;
          } else if (!(st[0] && st[1] && st[2]))
            Ti->m = partially_hidden;
              
          // check if some of vectices of Tj triangle are strictly inside 
          // the projected picture of Ti. it this is so, the triangle 
          // is marked as (at least) partially hidden
          
          if (Ti->m == visible)
            for (int k = 0; k < 3; ++k)           
              if (Ti->index_notin(Tj->indices[k]) && Ti->point_in(Tj->v[k])) {
                Ti->m = partially_hidden;
                break;
              }
        }
  }
  
  #else
    {
    int kk, i, j, Ntv = Tv.size();

    bool st[3];
    
    for (i = 1; i < Ntv; ++i)
      for (j = 0; j < i; ++j) // Loop over triangles in front of Ti
        if (Tv[j].m != hidden && Tv[i].bb_overlap(Tv[j])) { 
          
          // check if vectices of Ti-th triangle are obscured by a triangle 
          // in front of it. If can not be obscued if there are same as vertices 
          // of Ti
              
          for (int k = 0; k < 3; ++k)
            if ((st[k] = Vst[kk = Tv[i].indices[k]]) && 
                Tv[j].index_notin(kk) && 
                Tv[j].point_in(Tv[i].v[k]))
              st[k] = Vst[kk] = false;
          
          // if all vertices are not visible triangle is marked as hidden
          // if just some are hidden then it is marked as partially hidden
          if (!(st[0] || st[1] || st[2])) {
            Tv[i].m = hidden; 
            break;
          } else if (!(st[0] && st[1] && st[2]))
            Tv[i].m = partially_hidden;
              
          // check if some of vectices of Tj triangle are strictly inside 
          // the projected picture of Ti. it this is so, the triangle 
          // is marked as (at least) partially hidden
          
          if (Tv[i].m == visible)
            for (int k = 0; k < 3; ++k)           
              if (Tv[i].index_notin(Tv[j].indices[k]) && Tv[i].point_in(Tv[j].v[k])) {
                Tv[i].m = partially_hidden;
                break;
              }
        }
    
  }
  #endif
  
  // prepare M for Nt data
  M.clear(); 
  M.resize(Nt, hidden); // initially all triangles are hidden
  
  for (auto && tr : Tv) M[tr.index] = tr.m;
    
  delete [] Vst;
}

/*
  Support for the export of visible part of triangles in a mesh   
*/
template<class T>
struct T2Dpoint {
  T data[2];
  T2Dpoint(){}
  T2Dpoint(const T & x, const T &y) : data{x, y} {}
  T & operator[](const int &idx) { return data[idx]; }
  const T & operator[](const int &idx) const { return data[idx]; }
};

template<class T> using Tpath = std::vector<T2Dpoint<T> >;
template<class T> using Tpaths = std::vector<Tpath<T> >;


template<class T>
struct Ttrimesh {
  
  int index;
  
  std::vector<T3Dpoint<T> > V;
  std::vector<Ttriangle> Tr;
  
  Ttrimesh() {}
  Ttrimesh(int index):index(index){}
};

/*
  Determining the visibility ratio of triangles in a triangulated surfaces. 
  It can be a union of closed surfaces.  The algorithm is the sequence of

    * Back-face culling (only working for closed surfaces)
    * Painter's algorithm (depth ordering of triangles)
    * Determining the ratio of visible surface of triangles by 
      polygon algebra provided by a polygon clipping library. Its worst
      case relative precision is 1e-9.
  
  Comment:
  
  This algorithm has O(n^1.5) complexity, where n number of forward 
  facing triangles, but it has due to introduction of polygon algebra
  quite an overhead.
    
  Input:
    view[3] - direction of the observer
    
    V - vector of vertices used in triangles
    Tr - vector of triangles defined by indices vertices
    N - vector of normals to triangles (to speed up repeated use)

  Output:
    M - vector of the fractions of triangle that is visible
    Tph - triangulated surface of partially hidden triangles
*/


template <class T>
void triangle_mesh_visibility(
  double view[3], 
  std::vector<T3Dpoint<T> > & V,
  std::vector<Ttriangle> & Tr,
  std::vector<T3Dpoint<T> > & N,
  std::vector<T> & M,
  std::vector<Ttrimesh<T> > *Tph = 0
  ) {
 
  //
  // Defining the on-screen vector basis (t1,t2,view)
  //
  
  T b[3][3];
  
  create_basis(view, b);
  
  //
  // Back-face culling and storing all potentially visible triangles
  //
  
  int 
    Nt = Tr.size(), 
    Nv = V.size();
  
  // Sequence vertices in the on-screen basis
  // Vs = (x_0, y_0, z_0, x_1, y_1, z_1, ..., x_{Nv-1}, y_{Nv-1}, z_{Nv-1})
  T *Vs = new T [3*Nv];
  
  // If on-screen coordinates have been calculated
  // if ith vertex was projected onto screen bases Vst[i] = true
  std::vector<bool> Vst(Nv, false);
 
  // Prepare triangle to be sorted according to depth 
  struct Tt {
    
    int index;         // triangle index
         
    double z;               // maximal depth of the triangle
      
    Tt(){}
    
    Tt(const struct Tt & t): index(t.index), z(t.z) {}
    
    Tt(const int& index, const double &z):index(index), z(z) {}
    
    bool operator < (const Tt & rhs) const { return z > rhs.z; }
  };
  
  std::vector<int> Vi;  // indices of visual points
  std::vector<Tt> Tv;   // vector of potentially visible triangles
    
  //  Bounding box of all triangles on the screen
  T bb[4] = {
      +std::numeric_limits<T>::max(),
      -std::numeric_limits<T>::max(),
      +std::numeric_limits<T>::max(),
      -std::numeric_limits<T>::max()
    }; // {minX, maxX, minY, maxY}
   
  {
    int k, *t;
    
    T *n, *p, *v[3];
  
    for (int i = 0; i < Nt; ++i){
      
      n = N[i].data;  // normal of the ith triangle
      
      if (n[0]*view[0] + n[1]*view[1] + n[2]*view[2] > 0) {
        
        // indices of vectices used in the ith triangle
        t = Tr[i].indices;
        
        // calculate projection onto screen 
        for (int j = 0; j < 3; ++j) {
          
          v[j] = p = Vs + 3*(k = t[j]);
          
          if (!Vst[k]) {  // projecting on the screens 
            
            trans_basis(V[k].data, p, b);
            
            if (bb[0] > p[0]) bb[0] = p[0]; // Xmin
            if (bb[1] < p[0]) bb[1] = p[0]; // Xmax
            if (bb[2] > p[1]) bb[2] = p[1]; // Ymin
            if (bb[3] < p[1]) bb[3] = p[1]; // Ymax
            
            Vst[k] = true;
            Vi.push_back(k);
          }
        }
        
        Tv.emplace_back(i, utils::max3(v[0][2], v[1][2], v[2][2]));
      }
    }
  }
  
  Vst.clear();
  
  //
  // Lets do eclipsing
  //
  
  M.clear();
  M.resize(Nt, 0);
  
  if (Tv.size() == 0) {
    delete [] Vs;
    return;
  }
  
  //
  // Sort potentially visible triangles w.r.t. to max distance 
  // in direction of observation 
  //
  
  std::sort(Tv.begin(), Tv.end());
  
  //
  // Rescaling parameters
  //
  T scale = ClipperLib::hiRange,
    fac[4] = {
      2*scale/(bb[1] - bb[0]), (bb[0] + bb[1])/2,
      2*scale/(bb[3] - bb[2]), (bb[2] + bb[3])/2
    };
    
  //
  // Points on the screen in integers
  //
  std::vector<ClipperLib::IntPoint> VsI(Nv);
  
  {
    T *p;
    for (auto && i : Vi) {
      p = Vs + 3*i;
      VsI[i].X = fac[0]*(p[0] - fac[1]);
      VsI[i].Y = fac[2]*(p[1] - fac[3]);
    }
  }

  //
  //  Perform the eclipsing
  //
  {
    int *t;
  
    ClipperLib::Clipper c;    // clipping engine
        
    ClipperLib::Paths S, P;   // shadow (image on the screen) and remainder
  
    ClipperLib::Path s(3);    // triangle
    
    auto it = Tv.begin(), it_end = Tv.end();
  
    // add the first triangle to the shadow
    t = Tr[it->index].indices;
            
    for (int i = 0; i < 3; ++i) s[i] = VsI[t[i]];
  
    //if (!Orientation(s)) std::reverse(s.begin(), s.end());
    
    S.push_back(s);
    
    M[it->index] = 1;
    
    double tmp;
                  
    while (++it != it_end) {
      
      t = Tr[it->index].indices;
            
      for (int i = 0; i < 3; ++i) s[i] = VsI[t[i]];
      
      // Loading polygons
      c.Clear(); 
      c.AddPath(s, ClipperLib::ptSubject, true); // triangle T
      c.AddPaths(S, ClipperLib::ptClip, true);   // shadow  S
       
       // calculate remainder: P = T - S
      c.Execute(ClipperLib::ctDifference, P, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
      
      
      // detemine ratio of visibility
      // due to round off errors it can be slightly bigger than 1
      M[it->index] = tmp = std::min(1.0, Area(P)/std::abs(Area(s)));
      
      if (Tph && tmp > 0 && tmp < 1) {  // triangle is partially hidden
        //
        // convert P into real point polygon
        //
        Tpaths<double> paths(P.size());
        {
          auto it_ = paths.begin();
          
          for (auto && ps: P) {  
            it_->reserve(ps.size());
            for (auto && p : ps)
              it_->emplace_back(p.X/fac[0] + fac[1], p.Y/fac[2] + fac[3]);
            ++it_;
          }
        }
        
        // Triangulate polygon in manner that refines the mesh
        // https://en.wikipedia.org/wiki/Polygon_triangulation
        // http://stackoverflow.com/questions/5247994/simple-2d-polygon-triangulation
        // http://vterrain.org/Implementation/Libs/triangulate.html
        // http://mathworld.wolfram.com/Triangulation.html
        // 
        // Comments: 
        //  * Seidel's (fast, can handle holes, but triangles could be very sharp)
        //  http://www.cs.unc.edu/~dm/CODE/GEM/chapter.html
        //  https://github.com/jahting/pnltri.js
        //  https://github.com/palmerc/Seidel
        //
        //  * Ear clipping method (can't handle holes)
        //  https://www3.cs.stonybrook.edu/~skiena/392/programs/triangulate.c
        //  (with holes)
        //  https://www.geometrictools.com/Documentation/TriangulationByEarClipping.pdf
        //
        //  * Sweep Line Algorithm
        //  http://sites-final.uclouvain.be/mema/Poly2Tri/poly2tri.html
        // 
        //  * Sweep-line, Constrained Delaunay Triangulation
        //  https://github.com/greenm01/poly2tri <- !!!!
        //
        //  * Delaunay triangulations (optimal choice)
        //    https://www.cs.cmu.edu/~quake/triangle.html
        
        //
        // triangulate polygon in paths into (V2D, T)
        //
        
        
        Tph->emplace_back(it->index); 
        auto & R = Tph->back();           // results in (V,Tr)
        
        std::vector<T2Dpoint<T> > V2D;
        R.V.reserve(V2D.size());
        
        // go the map: paths->(V2D, R.T)  ????
        // WE NEED TO SETTLE FOR SOME SCHEME
        
        //
        // Projecting 2D triangles onto 3D original triangle
        //
        
       
        // storing vertices of the orginal triangle
        T *v2D[3], *v3D[3];  
        
        for (int i = 0; i < 3; ++i) {
          // 2D on screen
          v2D[i] = Vs + 3*t[i];
          // 3D
          v3D[i] = V[t[i]].data;
        }
  
        // reproject V2D onto the plane of the triangle
        T det, x[2], b[2], r[3], A[2][2]; 
        for (auto && u: V2D) {
          
          // define matrix A and vector b
          for (int i = 0; i < 2; ++i){
            b[i] = u[i] - v2D[0][i];
            
            for (int j = 0; j < 2; ++j) 
              A[i][j] = v2D[j+1][i] - v2D[0][i];
          } 
          det = A[0][0]*A[1][1] - A[0][1]*A[1][0];
          
          // solve 2x2 eq. A x = b 
          x[0] = (A[1][1]*b[0] - A[0][1]*b[1])/det;
          x[1] = (A[0][0]*b[1] - A[1][0]*b[0])/det;
          
          // points in a 3D triangle 
          for (int i = 0; i < 3; ++i) 
            r[i] =  v3D[0][i] + 
                    x[0]*(v3D[1][i]-v3D[0][i]) + 
                    x[1]*(v3D[2][i]-v3D[0][i]);
            
          R.V.emplace_back(r); 
        }
        
      }
          
      // calculet the union: S = S U T    
      c.Execute(ClipperLib::ctUnion, S, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
      
      // clean the shadow
      ClipperLib::CleanPolygons(S, 2);
    }
  }
  
  delete [] Vs;
}

#endif // #if !define(__eclipsing_h)
