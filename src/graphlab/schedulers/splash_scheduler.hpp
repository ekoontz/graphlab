/**
 * This class defines a Splash scheduler for Belief Propagation.
 * See
 *   Gonzalez, Low, Guestrin: Residual splash for optimally parallelizing belief propagation}},
 *   AISTATS,2009
 *
 **/

#ifndef GRAPHLAB_SPLASH_SCHEDULER_HPP
#define GRAPHLAB_SPLASH_SCHEDULER_HPP

#include <vector>
#include <map>
#include <algorithm>

#include <graphlab/util/mutable_queue.hpp>


#include <graphlab/graph/graph.hpp>
#include <graphlab/scope/iscope.hpp>

#include <graphlab/tasks/update_task.hpp>
#include <graphlab/schedulers/ischeduler.hpp>
#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/schedulers/support/direct_callback.hpp>
#include <graphlab/schedulers/support/vertex_task_set.hpp>

#include <graphlab/util/shared_termination.hpp>
#include <graphlab/util/optimal_termination.hpp>
#include <graphlab/util/dense_bitset.hpp>



#include <graphlab/macros_def.hpp>
namespace graphlab {

  template<typename Graph>
  class splash_scheduler : 
    public ischeduler<Graph> {
  public:
    typedef Graph graph_type;
    typedef ischeduler<Graph> base;

    typedef typename base::iengine_type iengine_type;
    typedef typename base::update_task_type update_task_type;
    typedef typename base::update_function_type update_function_type;
    typedef typename base::callback_type callback_type;
    typedef typename base::monitor_type monitor_type;

  private:
    using base::monitor;

  private:
    static const size_t queue_multiple = 5;

    // Typedefs --------------------------------------------------------------->
    
    /** the type of a splash which is just a vector of updates */
    typedef std::vector<vertex_id_t> splash_type;
    
    /** The type of the priority queue */
    typedef mutable_queue<size_t, double> pqueue_type;
        
  public:
    
    // Constructors / Destructors -------------------------------------------->
    splash_scheduler(iengine_type* engine,
                     Graph& graph, 
                     size_t ncpus) :     
      graph(graph),
      ncpus(ncpus),
      splash_size(100),
      update_fun(NULL),
      pqueues(ncpus * queue_multiple),
      queuelocks(ncpus * queue_multiple),
      vmap(graph.num_vertices()),
      splashes(ncpus), 
      splash_index(ncpus, 0),
      active_set(graph.num_vertices()),
      terminator(ncpus),
      callbacks(ncpus, direct_callback<Graph>(this, engine) ) {
      aborted = false;
      // Initialize the vertex map      
      for(size_t i = 0; i < vmap.size(); ++i) {
        vmap[i] = (i % pqueues.size());
      }
      // Do an extra shuffle
      // std::random_shuffle(vmap.begin(), vmap.end());
    } // end of constructor

   
    // Method ---------------------------------------------------------------->

    /** Called right before starting the engine */
    void start() {
      // Initialize the splashes
      for(size_t i = 0; i < ncpus; ++i)  rebuild_splash(i);
      terminator.reset();
    }
    
    //! Adds an update task with a particular priority
    void add_task(update_task_type task, double priority) {
      assert(task.function() == update_fun);
      assert(task.vertex() < graph.num_vertices());      
      vertex_id_t vertex = task.vertex();
      // Get the priority queue for the vertex
      size_t pqueue_id = vmap[vertex];

      // Grab the lock on that queue and promote
      queuelocks[pqueue_id].lock();
      // Add the vertex to the bitset
      bool already_present = active_set.set_bit(vertex);
      // If the vertex was not already present in the scheduler or the
      // vertex was present and still not in a splash (in the priority
      // queue) insert/promote it.
      if(!already_present ||
         pqueues[pqueue_id].contains(vertex)) {        
        // Insert/Promote the priority
        pqueues[pqueue_id].insert_max(vertex, priority);
        if (monitor != NULL)
          monitor->scheduler_task_added(task, priority);
      }
      // release the lock
      queuelocks[pqueue_id].unlock();
      // notify the termination condition that there is a new job
      size_t cpuid = pqueue_id /queue_multiple;
      assert(cpuid < ncpus);
      terminator.new_job(cpuid);
    }

    
    void add_tasks(const std::vector<vertex_id_t>& vertices, 
                   update_function_type func, double priority) {     
      foreach(vertex_id_t vertex, vertices) {
        add_task(update_task_type(vertex, func), priority);
      }
    }

    
    void add_task_to_all(update_function_type func, double priority) {
      update_fun = func;      
      for (vertex_id_t vertex = 0; vertex < graph.num_vertices(); 
           ++vertex){
        add_task(update_task_type(vertex, func), priority);
      }
    }
    
    callback_type& get_callback(size_t cpuid) {
      assert(cpuid < callbacks.size());
      return callbacks[cpuid];
    }

    

    sched_status::status_enum get_next_task(size_t cpuid, update_task_type &ret_task) {
      // While the scheduler is active
      while(true) {
        // Try and get next task for splash
        sched_status::status_enum ret = next_task_from_splash(cpuid, ret_task);
        // If we are not waiting then just return
        if (ret != sched_status::WAITING) {
          return ret;        
        } else {
          // Otherwise enter the shared terminator code
          terminator.begin_sleep_critical_section(cpuid);
          // Try once more to get the next task
          ret = next_task_from_splash(cpuid, ret_task);
          // If we are waiting then 
          if (ret != sched_status::WAITING) {
            // If we are either complete or succeeded then cancel the
            // critical section and return
            terminator.cancel_sleep_critical_section(cpuid);
            return ret;
          } else {
            // Otherwise end sleep waiting for either completion
            // (true) or some new work to become available.
            if (terminator.end_sleep_critical_section(cpuid)) {
              return sched_status::COMPLETE;
            }
          } 
        }
      } // End of while loop
      // We reach this point only if we are no longer active
      return sched_status::COMPLETE;
    }


    void scoped_modifications(size_t cpuid, vertex_id_t rootvertex,
                              const std::vector<edge_id_t>& updatededges){ }
                       
    void update_state(size_t cpuid, 
                      const std::vector<vertex_id_t>& updated_vertices,
                      const std::vector<edge_id_t>& updatededges) { }

    void completed_task(size_t cpuid, const update_task_type &task) { }


    
    void set_update_function(update_function_type fun) { update_fun = fun; }    
    void set_splash_size(size_t size) { splash_size = size; }

    void set_option(scheduler_options::options_enum opt, void* value) { 
      if (opt == scheduler_options::SPLASH_SIZE) {
        set_splash_size((size_t)(value));
      }
      else if (opt == scheduler_options::UPDATE_FUNCTION) {
        set_update_function((update_function_type)(value));
      }
      else {
        logger(LOG_WARNING, 
              "Splash Scheduler was passed an invalid option %d", opt);
      }
    };

    void abort() { aborted = true; }
    void restart() { 
      for (size_t i = 0;i < splashes.size(); ++i) {
        splashes[i].clear();
        splash_index[i] = 0;
      }
      aborted = false;
    }


  private:

    bool get_top(size_t cpuid, 
                 vertex_id_t& ret_vertex, double& ret_priority) {
      // starting at queue cpuid and running to the queue at index
      // cpuid + queue_multiple
      static size_t lastqid[128] = {0};
      
      for(size_t i = 0; i < queue_multiple; ++i) {
        size_t j = (i + lastqid[cpuid]) % queue_multiple;
        size_t index = cpuid * queue_multiple + j;
        queuelocks[index].lock();
        if(!pqueues[index].empty()) {
          // There is a top element in the task queue so we remove it
          // and take ownership
          ret_vertex = pqueues[index].top().first;
          ret_priority = pqueues[index].top().second;
          pqueues[index].pop();
          queuelocks[index].unlock();
          lastqid[cpuid] = j + 1;
          return true;
        }
        queuelocks[index].unlock();
      }
      lastqid[cpuid] = 0;
      return false;
    }
      
    void rebuild_splash(size_t cpuid) {
      assert(cpuid < splashes.size());    
      // Get a reference to the splash order for this processor
      splash_type& splash(splashes[cpuid]);
      // Ensure that the existing splash is depleted
      assert(splash_index[cpuid] == splash.size());
      // Clear out the splash
      splash.clear();
      splash_index[cpuid] = 0;
      // See if we can get a root
      vertex_id_t root(-1);
      double root_priority(0);        
      // Try and get a root
      bool root_found = get_top(cpuid, root, root_priority);
      // if a root was not found return
      if(!root_found) return;
      
      
      // Otherwise grow a splash starting at the root Splash growing
      // procedure
      // ----------------------------------------------->
      
      // We immideatly add the root to the splash updating the work 
      splash.push_back(root);
      size_t splash_work = work(root);
      if (root_priority > 1) splash_work = splash_size;
      // Initialize the set of visited vertices in the bfs and the bfs
      // queue
      std::set<vertex_id_t>    visited;
      std::queue<vertex_id_t>  bfs_queue;
      
      // Mark the root as visited
      visited.insert(root);
      
      // Add the roots neighbors to the BFS queue and mark them as
      // visited
      std::vector<edge_id_t> in_edge_ids =
        graph.in_edge_ids(root);
      std::random_shuffle(in_edge_ids.begin(), in_edge_ids.end());
      foreach(edge_id_t ineid, in_edge_ids) {
        vertex_id_t neighbor = graph.source(ineid);
        bfs_queue.push( neighbor );
        visited.insert( neighbor );
      }
      
      // Fill out the splash looping until the quota is achieved or
      // the tree becomes disconnected
      while( splash_work < splash_size  && !bfs_queue.empty() ) {
        // Get the top of the queue
        vertex_id_t vertex = bfs_queue.front(); bfs_queue.pop();      
        // Compute the work associated with the vertex
        size_t vertex_work = work(vertex);
        // If the vertex is too heavy then go to the next vertex
        if(vertex_work + splash_work > splash_size) continue;
        // Get the vertex from the priority queue
        queuelocks[vmap[vertex]].lock();
        bool success = pqueues[vmap[vertex]].remove(vertex);
        queuelocks[vmap[vertex]].unlock();
        // If the vertex was not in the queue then return
        if(!success) continue;                 
        // Otherwise we can add the vertex to the splash and update
        // the work
        splash.push_back(vertex);
        splash_work += vertex_work;
        // Add all the neighbors to the tree
        std::vector<edge_id_t> in_edge_ids =
          graph.in_edge_ids(vertex);
        std::random_shuffle(in_edge_ids.begin(), in_edge_ids.end());
        foreach(edge_id_t eid, in_edge_ids) {
          vertex_id_t neighbor = graph.source(eid);
          // if the neighbor has not been visited then add to the
          // bfs_queue
          if(visited.count(neighbor) == 0) {
            visited.insert(neighbor);
            bfs_queue.push(neighbor);
          }        
        } // end of if < splash size
      } // end of while loop   
      
      // Support reverse splashes ----------------------------------------------->
      size_t original_size = splash.size();
      if(original_size > 1) {
        std::reverse(splash.begin(), splash.end());
        // Extend the splash for the backwards pass
        splash.resize(original_size * 2 - 1);
        for(size_t i = 0; i < original_size - 1; i++) {
          splash.at(splash.size() - 1 - i) = splash.at(i);
        }
      } // end of if           
    } // end of rebuild splash

    
    //! Compute an estimate of the work associated with the vertex v
    size_t work(const vertex_id_t &v) const {
      return graph.in_edge_ids(v).size() + graph.out_edge_ids(v).size();
    }

    
    sched_status::status_enum next_task_from_splash(size_t cpuid, update_task_type &ret_task){
      assert(cpuid < splashes.size());
      // Loop until we either can't build a splash or we find an element
      while(true) {
        if (aborted) return sched_status::WAITING;
        // If the splash is depleted then start a new splash
        if((splash_index[cpuid] >= splashes[cpuid].size()) ) {
          rebuild_splash(cpuid);
        }
        
        // If we were unable to build a splash return waiting
        if(splash_index[cpuid] >= splashes[cpuid].size()) return sched_status::WAITING;
        
        // Otherwise loop until we obtian a vertex that is still
        // schedulable (in the active set) or we run out of vertices
        while(splash_index[cpuid] < splashes[cpuid].size()) {        
          vertex_id_t vertex =  splashes[cpuid][ splash_index[cpuid]++ ];
          // Clear the bit from the active set.  If the bit was        
          // previously set then we have succeeded and return the
          // new_task
          queuelocks[vmap[vertex]].lock();
          pqueues[vmap[vertex]].remove(vertex);
          queuelocks[vmap[vertex]].unlock();
          
          if( active_set.clear_bit(vertex) ) {            
            ret_task = update_task_type(vertex, update_fun);
            if (monitor != NULL)
              monitor->scheduler_task_scheduled(ret_task, 1.0);            
            return sched_status::NEWTASK;
          }
        }
      }
    }

    

    
    // Data Members ----------------------------------------------------------->
    
    Graph& graph;

    size_t ncpus;
    size_t splash_size;

    //! The update function (which must be first set)
    update_function_type update_fun;

    //! The vector of priority queue one for each processor
    std::vector< pqueue_type > pqueues;
    //! The locks for each queue
    std::vector< mutex >    queuelocks;
    //! The vertex max which maps each vertex to one of the queue
    std::vector< uint32_t >       vmap;
    
    //! The active splashes
    std::vector< splash_type > splashes;    
    //! The index of each splash
    std::vector< size_t > splash_index;

    //! Vertex task set used to track tasks that are currently active
    dense_bitset active_set;    

    //! Termination assessment object 
    shared_termination terminator;
    // optimal_termination terminator;


    /** The callbacks used to record update_tasks */
    std::vector<direct_callback<Graph> > callbacks;

    bool aborted;
    
  }; // End of splash scheduler

}; // end of graphlab namespace
#include <graphlab/macros_undef.hpp>
#endif
