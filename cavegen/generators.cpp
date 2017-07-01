#include "generators.h"
#include "game.h"
#include "imgui.h"
#include "imgui-sfml/imgui-sfml.h"
#include <algorithm>

#include <iostream>

#define ARRAYSIZE(_ARR) ((int)(sizeof(_ARR) / sizeof(*_ARR)))

void CellAutomataGenerator::init(const CellAutomataConfig& _config)
{
	config = _config;
	config.checkFunction = std::bind(&CellAutomataGenerator::basicEvaluation, this, std::placeholders::_1,std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
}

void CellAutomataGenerator::start(Map* map)
{
	noise(map);
}

void CellAutomataGenerator::generate(Map* map)
{
	step(map);
}

void CellAutomataGenerator::renderGUI(Game* game)
{
	const float SPINNER_DELTA = 0.05f;
	const float FAST_SPINNER_DELTA = 0.1f;

	ImGui::InputFloat("Initial wall probability [0..1]", &config.wallProbability, SPINNER_DELTA, FAST_SPINNER_DELTA);
	ImGui::InputInt("Neighbouring walls required for next iteration", &config.minSurroundingWallsForNextIter);
	ImGui::Checkbox("Count current cell?", &config.includeSelf);
	ImGui::Checkbox("Simulate borders?", &config.useCorners);
	const char* options[] = { "Basic", "Extended" };
	if (ImGui::Combo("Algorithm", &config.functionIndex, options, ARRAYSIZE(options)))
	{
		config.checkFunction = (config.functionIndex == 0) 
			? std::bind(&CellAutomataGenerator::basicEvaluation, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)
			: std::bind(&CellAutomataGenerator::basicEvaluationClosed, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
	}
}

void CellAutomataGenerator::noise(Map* map)
{
	int numCells = map->numCells();
	std::random_device r;
	std::default_random_engine engine(r());
	std::uniform_real_distribution<float> dist(0, 1);

	for (int i = 0; i < map->rows; ++i)
	{
		for (int j = 0; j < map->cols; ++j)
		{
			bool corner = j == 0 || j == map->cols - 1 || i == 0 || i == map->rows - 1;
			if (!config.useCorners && corner)
			{
				(*map)(i, j) = CellType::Wall;
			}
			else
			{
				(*map)(i, j) = dist(engine) <= config.wallProbability ? CellType::Wall : CellType::Empty;
			}			
		}
	}
	
}

void CellAutomataGenerator::step(Map* map)
{
	std::vector<CellType> copy(map->cells);
	int startRow = config.useCorners ? 0 : 1;
	int endRow = config.useCorners ? map->rows : map->rows - 1;
	int startCol = config.useCorners ? 0 : 1;
	int endCol = config.useCorners ? map->cols : map->cols - 1;
	
	for (int i = startRow; i < endRow; ++i)
	{
		for (int j = startCol; j < endCol; ++j)
		{
			(*map)(i, j) = config.checkFunction(i, j, map, copy) ? CellType::Wall : CellType::Empty;
		}
	}
}

bool CellAutomataGenerator::basicEvaluation(int i, int j, const Map* map, const Map::CellArray& oldMapCells)
{
	int numWalls = countNeighbourhood(i, j, map, oldMapCells, 1, config.includeSelf);
	return numWalls >= config.minSurroundingWallsForNextIter;
}

bool CellAutomataGenerator::basicEvaluationClosed(int i, int j, const Map* map, const Map::CellArray& oldCells)
{
	bool basic = basicEvaluation(i, j, map, oldCells);
	int numWalls = countNeighbourhood(i, j, map, oldCells, 2, config.includeSelf);
	return basic || numWalls <= 1;
}

int CellAutomataGenerator::countNeighbourhood(int i, int j, const Map* map, const Map::CellArray& oldCells, int distance, bool countSelf)
{
	int numWalls = 0;
	for (int deltaRow = -distance; deltaRow <= distance; ++deltaRow)
	{
		for (int deltaCol = -distance; deltaCol <= distance; ++deltaCol)
		{
			if (distance == 2 && (abs(i - deltaRow) == 2 && abs(j - deltaCol) == 2)) continue;
			if (!map->validCoords(i + deltaRow, j + deltaCol)) continue;
			if (deltaRow == 0 && deltaCol == 0 && !countSelf) continue;
			if (oldCells[map->asIndex(i + deltaRow, j + deltaCol)] == CellType::Wall)
			{
				numWalls++;
			}
		}
	}
	return numWalls;
}

//----------------------------

DrunkardWalkGenerator::DrunkardWalkGenerator()
:r()
,engine(r())
{
}
void DrunkardWalkGenerator::init(const DrunkardWalkConfig& _config)
{
	config = _config;
}
void DrunkardWalkGenerator::renderGUI(Game* game)
{
	ImGui::InputFloat("Expected empty ratio (0..1)", &config.expectedRatio);
}
void DrunkardWalkGenerator::start(Map* map)
{
	for (int i = 0; i < map->rows; ++i)
	{
		for (int j = 0; j < map->cols; ++j)
		{
			(*map)(i, j) = CellType::Wall;
		}
	}

	std::uniform_int_distribution<int> rowDist(0, map->rows - 1);
	std::uniform_int_distribution<int> colDist(0, map->cols - 1);

	row = rowDist(engine);
	col = colDist(engine);
	(*map)(row, col) = CellType::Empty;

}

void DrunkardWalkGenerator::step(Map* map)
{
	static const int rowIndexes[] = { 0, 0, 1, -1 };
	static const int colIndexes[] = { 1, -1, 0, 0 };

	std::uniform_int_distribution<int> direction(0, 3);
	int dir = -1;
	do
	{
		dir = direction(engine);
	} while (dir < 0 || !map->validCoords(row + rowIndexes[dir], col + colIndexes[dir]));

	row += rowIndexes[dir];
	col += colIndexes[dir];
	(*map)(row, col) = CellType::Empty;
}
void DrunkardWalkGenerator::generate(Map* map)
{
	
	int countFloor = std::count(map->cells.begin(), map->cells.end(), CellType::Empty);
	float ratio = countFloor / (float)map->numCells();

	while (ratio < config.expectedRatio)
	{
		step(map);
		
		countFloor = std::count(map->cells.begin(), map->cells.end(), CellType::Empty);
		ratio = countFloor / (float)map->numCells();
	}
}

//----------------------------

BSPTree::BSPTree()
	:left(nullptr)
	,right(nullptr)
	,area({ 0,0,0,0 })
{

}

BSPTree::~BSPTree()
{
	if (left) delete left;
	if (right) delete right;
	if (room) delete room;
	left = right = nullptr;
	room = nullptr;
}

void BSPTree::getLeaves(std::vector<BSPTree*>& leaves)
{
	if (right == nullptr && left == nullptr)
	{
		leaves.emplace_back(this);
	}
	else
	{
		left->getLeaves(leaves);
		right->getLeaves(leaves);
	}
}

BSPTree* BSPTree::getRoom()
{
	if (room != nullptr)
	{
		return this;
	}
	else
	{
		BSPTree* leftRoom = nullptr;
		BSPTree* rightRoom = nullptr;
		if (left != nullptr)
		{
			leftRoom = left->getRoom();
		}
		if (right != nullptr)
		{
			rightRoom = right->getRoom();
		}

		if (leftRoom == nullptr && rightRoom == nullptr) return nullptr;
		if (!leftRoom) return rightRoom;
		if (!rightRoom) return leftRoom;
		std::uniform_real_distribution<float> dist(0.f, 1.f);
		return dist(*splitEngine) < 0.5f ? leftRoom : rightRoom;
	}
}

bool BSPTree::split()
{
	int splitValue;

	std::uniform_real_distribution<float> realDist(0.f, 1.f);
	bool isHSplit = realDist(*splitEngine) < config->horizSplitProbability;
	float ratio = area.w / (float)area.h;
	if (ratio >= (1.f + config->splitVRatio))
	{
		isHSplit = false;
	}
	else if ((area.h / (float)area.w) > 1.f + config->splitHRatio)
	{
		isHSplit = true;
	}

	int maxSize = 0;
	int minSize = 0;
	if (isHSplit)
	{
		maxSize = area.h - config->minHeight;
		minSize = config->minHeight;
	}
	else
	{
		maxSize = area.w - config->minWidth;
		minSize = config->minWidth;
	}
	if (maxSize <= minSize)
	{
		return false;
	}

	std::uniform_int_distribution<int> splitDist(minSize, maxSize);
	splitValue = splitDist(*splitEngine);
		
	left = new BSPTree();
	right = new BSPTree();
	left->splitEngine = right->splitEngine = splitEngine;
	left->config = right->config = config;
	if (isHSplit)
	{
		left->area = {area.x, area.y, area.w, splitValue};
		right->area = { area.x, area.y + splitValue, area.w, area.h - splitValue };
	}
	else
	{
		left->area = {area.x, area.y, splitValue, area.h};
		right->area = { area.x + splitValue, area.y, area.w - splitValue, area.h };
	}
	left->split();
	right->split();
	return true;
}

BSPGenerator::BSPGenerator()
{
}

BSPGenerator::~BSPGenerator()
{
	if (generatedTree) { delete generatedTree; generatedTree = nullptr; }
}
void BSPGenerator::init(const BSPConfig& _config)
{
	config = _config;
	splitEngine.seed(r());
}
void BSPGenerator::renderGUI(Game* game)
{
	
}
void BSPGenerator::start(Map* map)
{
	for (int i = 0; i < map->rows; ++i)
	{
		for (int j = 0; j < map->cols; ++j)
		{
			(*map)(i, j) = CellType::Wall;
		}
	}

	generatedTree = new BSPTree();
	generatedTree->left = generatedTree->right = nullptr;
	generatedTree->area = { 0, 0, map->cols, map->rows };
	
	generatedTree->splitEngine = &splitEngine;
	generatedTree->config = &config;
}

void BSPGenerator::step(Map* map)
{
}

void carveHall(const BSPRect& coords, Map* map)
{
	for (int r = coords.y; r <= coords.y + coords.h; ++r)
	{
		for (int c = coords.x; c <= coords.x + coords.w; ++c)
		{
			(*map)(r, c) = CellType::Empty;
		}
	}
}

void findHall(std::default_random_engine& engine, BSPRect* rect1, BSPRect* rect2, Map* map)
{
	typedef std::uniform_int_distribution<int> IntRangeGen;
	
	IntRangeGen dist(rect1->x + 1, rect1->x + rect1->w - 1);
	std::uniform_real_distribution<float> dirDist(0.f, 1.f);

	int c1 = dist(engine);
	dist = IntRangeGen(rect1->y + 1, rect1->y + rect1->h - 1);
	int r1 = dist(engine);

	dist = IntRangeGen(rect2->x + 1, rect2->x + rect2->w - 1);
	int c2 = dist(engine);
	dist = IntRangeGen(rect2->y + 1, rect2->y + rect2->h - 1);
	int r2 = dist(engine);

	int deltaR = r2 - r1;
	int deltaC = c2 - c1;

	BSPRect hallRect;
	BSPRect hallRectAux;

	if (deltaC < 0)
	{
		if (deltaR > 0)
		{
			bool upCorner = dirDist(engine) < 0.5f;
			if (upCorner)
			{
				carveHall({ c2,r1,-deltaC,1 }, map);
				carveHall({ c2,r1, 1, deltaR }, map);
			}
			else
			{
				carveHall({ c1,r1,1,deltaR }, map);
				carveHall({ c2,r2, -deltaC, 1}, map);
			}
		}
		else if (deltaR < 0)
		{
			bool upCorner = dirDist(engine) < 0.5f;
			if (upCorner)
			{
				carveHall({ c1,r2,1,-deltaR }, map);
				carveHall({ c2,r2, -deltaC, 1}, map);
			}
			else
			{
				carveHall({ c2,r1,-deltaC,1}, map);
				carveHall({ c2,r2, 1, -deltaR }, map);
			}
		}
		else
		{
			carveHall({c2,r2,-deltaC,1}, map);
		}
	}
	else if (deltaC > 0)
	{
		if (deltaR > 0)
		{
			bool upCorner = dirDist(engine) < 0.5f;
			if (upCorner)
			{
				carveHall({c2,r1,1,deltaR}, map);
				carveHall({c1,r1,deltaC,1}, map);				
			}
			else
			{
				carveHall({ c1,r2,deltaC,1 }, map);
				carveHall({ c1,r1,1,deltaR }, map);
			}
		}
		else if (deltaR < 0)
		{
			bool upCorner = dirDist(engine) < 0.5f;
			if (upCorner)
			{
				carveHall({c1,r2,1,-deltaR}, map);
				carveHall({c1,r2,deltaC,1}, map);
			}
			else
			{
				carveHall({c1,r1,deltaC,1}, map);
				carveHall({c2,r2,1,-deltaR}, map);
			}
		}
		else
		{
			carveHall({ c1,r1,deltaC,1}, map);
		}
	}
	else
	{
		if (deltaR > 0)
		{
			carveHall({c1,r1,1,deltaR}, map);
		}
		else if (deltaR < 0)
		{
			carveHall({c1,r2,1,-deltaR}, map);
		}
	}
}

void createHalls(std::default_random_engine& engine, BSPTree* node, Map* map)
{
	if (node->left != nullptr || node->right != nullptr)
	{
		if (node->left != nullptr)
		{
			createHalls(engine, node->left, map);
		}

		if (node->right != nullptr)
		{
			createHalls(engine, node->right, map);
		}

		if (node->left != nullptr && node->right != nullptr)
		{
			BSPTree* lRoom = node->left->getRoom();
			BSPTree* rRoom = node->right->getRoom();
			if (lRoom != nullptr && rRoom != nullptr)
			{
				findHall(engine, lRoom->room, rRoom->room, map);
			}
		}
	}
}
void BSPGenerator::generate(Map* map)
{
	std::vector<BSPTree*> leaves;
	
	// 1. Split
	generatedTree->split();
	
	
	// 2: populate
	std::uniform_real_distribution<float> skipDist(0.f, 1.f);
	
	generatedTree->getLeaves(leaves);
	for (BSPTree* leaf : leaves)
	{
		bool willSkip = skipDist(splitEngine) < config.emptyRoomProbability;
		if (willSkip) continue;

		std::uniform_int_distribution<int> roomDist(config.minRoomHeight, config.maxRoomHeight);
		int h = roomDist(splitEngine);
		roomDist = std::uniform_int_distribution<int>(config.minRoomWidth, config.maxRoomWidth);
		int w = roomDist(splitEngine);

		roomDist = std::uniform_int_distribution<int>(1, leaf->area.w - w - 1);
		int x = leaf->area.x + roomDist(splitEngine);

		roomDist = std::uniform_int_distribution<int>(1, leaf->area.h - h - 1);
		int y = leaf->area.y + roomDist(splitEngine);
		
		leaf->room = new BSPRect();
		leaf->room->x = x;
		leaf->room->y = y; 
		leaf->room->w = w; 
		leaf->room->h = h;

		for (int posY = y; posY <= y + h; ++posY)
		{
			for (int posX = x; posX <= x + w; ++posX)
			{
				(*map)(posY, posX) = CellType::Empty;
			}
		}
	}
	
	// 3: connect
	createHalls(splitEngine, generatedTree, map);
}

