#include <map>
#include "cola.h"
using namespace std;

namespace cola {
    namespace ccomponents {
        struct Node {
            unsigned id;
            bool visited;
            vector<Node*> neighbours;
            Rectangle* r;
        };
        // Depth first search traversal of graph to find connected component
        void dfs(Node* v,
                set<Node*>& remaining,
                Component* component,
                map<unsigned,pair<Component*,unsigned> > &cmap) {
            v->visited=true;
            remaining.erase(v);
            cmap[v->id]=make_pair(component,component->node_ids.size());
            component->node_ids.push_back(v->id);
            component->rects.push_back(v->r);
            for(unsigned i=0;i<v->neighbours.size();i++) {
                Node* u=v->neighbours[i];
                if(!u->visited) {
                    dfs(u,remaining,component,cmap);
                }
            }
        }
    }

    using namespace ccomponents;

    // for a graph of n nodes, return connected components
    void connectedComponents(
            const vector<Rectangle*> &rs,
            const vector<Edge> &es, 
            const SimpleConstraints &scx,
            const SimpleConstraints &scy,
            vector<Component*> &components) {
        unsigned n=rs.size();
        vector<Node> vs(n);
        set<Node*> remaining;
        for(unsigned i=0;i<n;i++) {
            vs[i].id=i;
            vs[i].visited=false;
            vs[i].r=rs[i];
            remaining.insert(&vs[i]);
        }
        for(vector<Edge>::const_iterator e=es.begin();e!=es.end();e++) {
            vs[e->first].neighbours.push_back(&vs[e->second]);
            vs[e->second].neighbours.push_back(&vs[e->first]);
        }
        map<unsigned,pair<Component*,unsigned> > cmap;
        while(!remaining.empty()) {
            Component* component=new Component;
            Node* v=*remaining.begin();
            dfs(v,remaining,component,cmap);
            components.push_back(component);
        }
        for(vector<Edge>::const_iterator e=es.begin();e!=es.end();e++) {
            pair<Component*,unsigned> u=cmap[e->first],
                                      v=cmap[e->second];
            assert(u.first==v.first);
            u.first->edges.push_back(make_pair(u.second,v.second));
        }
        for(SimpleConstraints::const_iterator ci=scx.begin();ci!=scx.end();ci++) {
            SimpleConstraint *c=*ci;
            pair<Component*,unsigned> u=cmap[c->left],
                                      v=cmap[c->right];
            assert(u.first==v.first);
            u.first->scx.push_back(
                    new SimpleConstraint(u.second,v.second,c->gap));
        }
        for(SimpleConstraints::const_iterator ci=scy.begin();ci!=scy.end();ci++) {
            SimpleConstraint *c=*ci;
            pair<Component*,unsigned> u=cmap[c->left],
                                      v=cmap[c->right];
            assert(u.first==v.first);
            u.first->scy.push_back(
                    new SimpleConstraint(u.second,v.second,c->gap));
        }
    }
}
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
