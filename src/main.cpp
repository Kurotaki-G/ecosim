#define CROW_MAIN
#define CROW_STATIC_DIR "../public"

#include "crow_all.h"
#include "json.hpp"
#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>
//#include <barrier>

int n_threads = 0;
std::mutex m;
std::mutex take_action;
std::condition_variable new_iteration;
std::condition_variable thread_finished;
std::mutex ni_m;
std::mutex tf_m;

static const uint32_t NUM_ROWS = 15;

// Constants
const uint32_t PLANT_MAXIMUM_AGE = 10;
const uint32_t HERBIVORE_MAXIMUM_AGE = 50;
const uint32_t CARNIVORE_MAXIMUM_AGE = 80;
const uint32_t MAXIMUM_ENERGY = 200;
const uint32_t THRESHOLD_ENERGY_FOR_REPRODUCTION = 20;

// Probabilities
const double PLANT_REPRODUCTION_PROBABILITY = 0.2;
const double HERBIVORE_REPRODUCTION_PROBABILITY = 0.075;
const double CARNIVORE_REPRODUCTION_PROBABILITY = 0.025;
const double HERBIVORE_MOVE_PROBABILITY = 0.7;
const double HERBIVORE_EAT_PROBABILITY = 0.9;
const double CARNIVORE_MOVE_PROBABILITY = 0.5;
const double CARNIVORE_EAT_PROBABILITY = 1.0;

// Type definitions
enum entity_type_t
{
    empty,
    plant,
    herbivore,
    carnivore
};

struct pos_t
{
    uint32_t i;
    uint32_t j;
};

struct entity_t
{
    entity_type_t type;
    int32_t energy;
    int32_t age;
};

// Auxiliary code to convert the entity_type_t enum to a string
NLOHMANN_JSON_SERIALIZE_ENUM(entity_type_t, {
                                                {empty, " "},
                                                {plant, "P"},
                                                {herbivore, "H"},
                                                {carnivore, "C"},
                                            })

// Auxiliary code to convert the entity_t struct to a JSON object
namespace nlohmann
{
    void to_json(nlohmann::json &j, const entity_t &e)
    {
        j = nlohmann::json{{"type", e.type}, {"energy", e.energy}, {"age", e.age}};
    }
}

// Function to generate a random position based on probability
int random_position(int max_) {
    return rand() % max_ + 1;;
}

// Grid that contains the entities
static std::vector<std::vector<entity_t>> entity_grid;


bool check_age(entity_t* entity){
    int max_age = 0;

    switch (entity -> type){
        case plant: max_age = PLANT_MAXIMUM_AGE;
        case herbivore: max_age = HERBIVORE_MAXIMUM_AGE;
        case carnivore: max_age = CARNIVORE_MAXIMUM_AGE;
    }

    if(entity -> age > max_age) then: return false;

    return true;
}


void iteracao(pos_t pos){
    
    //tratar primeiro as mortes, alimentações e reproduções
    //por último se ele vai andar
    //deve haver um lock no inicio 
    //deve haver um wait() caso a ação seja de reprodução ou andar
    entity_t* entity;

    bool isAlive = true;
    while(isAlive){

        // Cria um objeto do tipo unique_lock que no construtor chama m.lock()
		std::unique_lock<std::mutex> ni_lk(ni_m);

        new_iteration.wait(ni_lk);


        entity = &entity_grid[pos.i][pos.j];


        entity -> age++;

        isAlive = check_age(entity);

        thread_finished.notify_one();

    }
    n_threads--;
}


int main()
{
    crow::SimpleApp app;

    // Endpoint to serve the HTML page
    CROW_ROUTE(app, "/")
    ([](crow::request &, crow::response &res)
     {
        // Return the HTML content here
        res.set_static_file_info_unsafe("../public/index.html");
        res.end(); });

    CROW_ROUTE(app, "/start-simulation")
        .methods("POST"_method)([](crow::request &req, crow::response &res)
                                { 
        // Parse the JSON request body
        nlohmann::json request_body = nlohmann::json::parse(req.body);

       // Validate the request body 
        uint32_t total_entinties = (uint32_t)request_body["plants"] + (uint32_t)request_body["herbivores"] + (uint32_t)request_body["carnivores"];
        if (total_entinties > NUM_ROWS * NUM_ROWS) {
        res.code = 400;
        res.body = "Too many entities";
        res.end();
        return;
        }

        // Clear the entity grid
        entity_grid.clear();
        entity_grid.assign(NUM_ROWS, std::vector<entity_t>(NUM_ROWS, { empty, 0, 0}));
        
        // Create the entities
        // <YOUR CODE HERE>
        
        int num_p = (uint32_t)request_body["plants"];
        int num_h = (uint32_t)request_body["herbivores"];
        int num_c = (uint32_t)request_body["carnivores"];

        for (int i = 0; i<num_p+num_h+num_c; i++){
            pos_t pos;
            pos.i = random_position(NUM_ROWS-1);
            pos.j = random_position(NUM_ROWS-1);
            entity_t entity;
            entity.type = entity_grid[pos.i][pos.j].type;
            if(entity.type == empty){
                if(i<num_p){
                    entity.type = plant;
                }
                else if(i<num_p+num_h){
                    entity.type = herbivore;
                    entity.energy = 100;
                }
                else{
                    entity.type = carnivore;
                    entity.energy = 100;
                }
                entity.age = 0;
                entity_grid[pos.i][pos.j] = entity;
                std::thread t(iteracao,pos);
                 t.detach();
                 n_threads++;
            }
            else{
                i--;
            }
        }

        // Return the JSON representation of the entity grid
        nlohmann::json json_grid = entity_grid; 
        res.body = json_grid.dump();
        res.end(); });

    // Endpoint to process HTTP GET requests for the next simulation iteration
    CROW_ROUTE(app, "/next-iteration")
        .methods("GET"_method)([]()
                               {
        // Simulate the next iteration
        // Iterate over the entity grid and simulate the behaviour of each entity
        
        // <YOUR CODE HERE>
        new_iteration.notify_all();

        int n_threads_aux = n_threads;
        int n_ready_threads = 0;

        while(n_ready_threads < n_threads_aux){
            std::unique_lock<std::mutex> tf_lk(tf_m);
            thread_finished.wait(tf_lk);
            n_ready_threads++;
        }

        // Return the JSON representation of the entity grid
        nlohmann::json json_grid = entity_grid; 
        return json_grid.dump(); });
    app.port(8080).run();

    return 0;
}