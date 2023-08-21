/**
 *  Copyright (c) 2015 by Contributors
 */
#ifndef PS_INTERNAL_VAN_H_
#define PS_INTERNAL_VAN_H_
#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <memory>
#include <atomic>
#include <ctime>
#include <utility>
#include <functional>
#include <future>
#include <ctime>
#include <sys/time.h>
#include <unordered_set>
#include <condition_variable>
#include "ps/base.h"
#include "ps/internal/message.h"
#include "ps/internal/threadsafe_queue.h"
namespace ps {
class Resender;

/**
 * \brief Van sends messages to remote nodes
 *
 * If environment variable PS_RESEND is set to be 1, then van will resend a
 * message if it no ACK messsage is received within PS_RESEND_TIMEOUT millisecond
 */
class Van {
 public:
    /**
     * \brief create Van
     * \param type zmq, socket, ...
     */
    static Van *Create(const std::string &type);

    /** \brief constructer, do nothing. use \ref Start for real start */
    Van() {}

    /**\brief deconstructer, do nothing. use \ref Stop for real stop */
    virtual ~Van() {}

    /**
     * \brief start van
     *
     * must call it before calling Send
     *
     * it initalizes all connections to other nodes.  start the receiving
     * threads, which keeps receiving messages. if it is a system
     * control message, give it to postoffice::manager, otherwise, give it to the
     * accoding app.
     */
    virtual void Start(int customer_id);

    /**
     * \brief send a message, It is thread-safe
     * \return the number of bytes sent. -1 if failed
     */
    int Send(Message &msg);
    void Send_transmit(Message &msg);
    void Push(Message &msg);
    void Sending(int node_id);
    void RecvReply(Message &msg);
    void ProcessRecvReplyCommand(Message &msg);
    void Route_Manager();
    void ProcessSpeedReportCommand(Message &msg);
    float check_change(float temp[9][9]);
    void check_trace();
    std::vector<int> MulticastTreeBuild();
    void ProcessReply2Command(Message &msg);
    std::vector<std::vector<float>> PolicyDecision();
    void route_update2();
    void RelationUpdate();
    void EnableSendCheck();
    /**
     * \brief return my node
     */
    inline const Node &my_node() const {
      CHECK(ready_) << "call Start() first";
      return my_node_;
    }

    /**
     * \brief stop van
     * stop receiving threads
     */
    virtual void Stop();

    /**
     * \brief get next available timestamp. thread safe
     */
    inline int GetTimestamp() { return timestamp_++; }

    /**
     * \brief whether it is ready for sending. thread safe
     */
    inline bool IsReady() { return ready_; }

    void Add_trace(Message &msg);
    Node my_node_;
    int my_container_id;
    std::vector<Node> my_node_group;
    int links[9][9]={ {2,3,0,0,0,0,0,0,0},
                      {3,1,5,0,0,0,0,0,0},
                      {1,2,4,0,0,0,0,0,0},
                      {3,5,6,0,0,0,0,0,0},
                      {2,7,4,0,0,0,0,0,0},
                      {4,7,8,9,0,0,0,0,0},
                      {5,6,8,0,0,0,0,0,0},
                      {6,7,9,0,0,0,0,0,0},
                      {6,8,0,0,0,0,0,0,0}
                      //{1,2,3,4,5,6,7,8,9}
                       };

    std::vector<std::vector<Node>> eth_sort;
    bool perf_update_flag=false;
    std::vector<float> perf;
    double update_rate;
    double report_rate;
    int num_msg_report;
    bool enable_random;
    bool enable_muti_trace;
    int check_period;
    bool init_flag=true;
    int in_degree;
    std::vector<int> neighbors;
    std::vector<int> parents_temp;
    std::vector<std::vector<int>> sons_temp;
    std::vector<int> parents;
    std::vector<std::vector<int>> sons;
    int iters=0;
    int cache_count=0;
    int enable_send=1;
    int notify_count=0;
    std::unordered_map<ps::Key, int> key_timestamp;
    int num_enable_server;
    int size_msg_report;
    int size_cache;
    std::vector<std::vector<std::vector<std::vector<int>>>> route;
    float mid_tp=1;
    bool change_flag=false;
    bool init_tree =true; 

 protected:
    /**
     * \brief connect to a node
     */
    virtual void Connect(const Node &node) = 0;
    virtual void Connect_Eth(Node &node_,Node &node) = 0;
    /**
     * \brief bind to my node
     * do multiple retries on binding the port. since it's possible that
     * different nodes on the same machine picked the same port
     * \return return the port binded, -1 if failed.
     */
    virtual int Bind(const Node &node, int max_retry) = 0;

    /**
     * \brief block until received a message
     * \return the number of bytes received. -1 if failed or timeout
     */
    virtual int RecvMsg(Message *msg, int rece_eth_id) = 0;

    /**
     * \brief send a mesage
     * \return the number of bytes sent
     */
    virtual int SendMsg(const Message &msg) = 0;

    /**
     * \brief pack meta into a string
     */
    void PackMeta(const Meta &meta, char **meta_buf, int *buf_size);

    /**
     * \brief unpack meta from a string
     */
    void UnpackMeta(const char *meta_buf, int buf_size, Meta *meta);

    bool i_in_trace(int i, std::vector<int> trace) ;
    void find_trace(int start, int end, float max_tp, std::vector<int> trace);
    int min(std::vector<float> max_tps);
    void add_route();
    void compute_performance();
    void route_update();
    int x_in_array(int x, std::vector<int> array);

    Node scheduler_;

    bool is_scheduler_;
    std::mutex start_mu_;
    std::mutex eth_mu;
    std::mutex send_mu;
    std::vector<bool> send_flag;
    std::condition_variable cond_send;
    std::vector<Node> eth_node;
    int num_report_eth=0;
    Meta nodes;
    Meta recovery_nodes;  // store recovery nodes
    int num_nodes = 9;
    std::vector<std::vector<double>> speeds;
    std::vector<float> max_tps;
    std::vector<std::vector<int>> traces;
    std::vector<int> mid_nodes;
    std::vector<bool> receive_flag;
    int second_queue_limit;
    float tps[9][9] = { {0,1,1,0,0,0,0,0,0},
                        {1,0,1,0,1,0,0,0,0},
                        {1,1,0,1,0,0,0,0,0},
                        {0,0,1,0,1,1,0,0,0},
                        {0,1,0,1,0,0,1,0,0},
                        {0,0,0,1,0,0,1,1,1},
                        {0,0,0,0,1,1,0,1,0},
                        {0,0,0,0,0,1,1,0,1},
                        {0,0,0,0,0,1,0,1,0} };

    std::unordered_map<int, int> route2next;
    void ProcessRouteReleaseCommand(Message& msg);
    std::vector<std::vector<int>> route2dst;
    struct timeval t1,t2;
 private:
    std::unordered_map<int,int> send_queue_map;
    std::vector<ThreadsafeQueue*> send_queue_group;
    ThreadsafeQueue send_cache;
    Node Init_eth_group(int eth);
    /** thread function for receving */
    void Receiving(int rece_eth_id);
    void StartReceivings();

    /** thread function for heartbeat */
    void Heartbeat();

    // node's address string (i.e. ip:port) -> node id
    // this map is updated when ip:port is received for the first time
    std::unordered_map<std::string, int> connected_nodes_;
    // maps the id of node which is added later to the id of node
    // which is with the same ip:port and added first
    std::unordered_map<int, int> shared_node_mapping_;
    std::unordered_map<int, int> node_container_id;

    /** whether it is ready for sending */
    std::atomic<bool> ready_{false};
    std::atomic<size_t> send_bytes_{0};
    size_t recv_bytes_ = 0;
    int num_servers_ = 0;
    int num_workers_ = 0;
    /** the thread for receiving messages */
    std::unique_ptr<std::thread> receiver_thread_;
    std::vector<std::unique_ptr<std::thread>> receiver_thread_group;
    /** the thread for sending heartbeat */
    std::unique_ptr<std::thread> heartbeat_thread_;
    std::unique_ptr<std::thread> route_thread;
    std::vector<std::unique_ptr<std::thread>> sender_thread_group;
    std::vector<int> barrier_count_;
    /** msg resender */
    Resender *resender_ = nullptr;
    int drop_rate_ = 0;
    std::atomic<int> timestamp_{0};
    int init_stage = 0;

    /**
     * \brief processing logic of AddNode message for scheduler
     */
    void ProcessAddNodeCommandAtScheduler(Message* msg, Meta* nodes, Meta* recovery_nodes);

    /**
     * \brief processing logic of Terminate message
     */
    void ProcessTerminateCommand();
    void ProcessReply1Command(Message &msg);
    /**
     * \brief processing logic of AddNode message (run on each node)
     */
    void ProcessAddNodeCommand(Message* msg, Meta* nodes, Meta* recovery_nodes);

    /**
     * \brief processing logic of Barrier message (run on each node)
     */
    void ProcessBarrierCommand(Message* msg);

    /**
     * \brief processing logic of AddNode message (run on each node)
     */
    void ProcessHearbeat(Message* msg);

    /**
     * \brief processing logic of Data message
     */
    void ProcessDataMsg(Message* msg);
    //precess ask command
    void ProcessReportEthCommand(Message* msg);
    void ProcessReportReplyCommand(Message* msg);
    /**
     * \brief called by ProcessAddNodeCommand, in scheduler it assigns an id to the
     *        newly added node; in other nodes, it updates the node id with what is received
     *        from scheduler
     */
    void UpdateLocalID(Message* msg, std::unordered_set<int>* deadnodes_set, Meta* nodes,
                       Meta* recovery_nodes);

    const char *heartbeat_timeout_val = Environment::Get()->find("PS_HEARTBEAT_TIMEOUT");
    int heartbeat_timeout_ = heartbeat_timeout_val ? atoi(heartbeat_timeout_val) : 0;

    DISALLOW_COPY_AND_ASSIGN(Van);
};
}  // namespace ps
#endif  // PS_INTERNAL_VAN_H_
