/* $CVSID$ */ 
#include "stdafx.h"
#include "entity.h"
#include "actor.h"
#include "world.h"
#include "net_driver.h"

#include "collider.h"
#include "reporter.h"

#include "md3.h"
#include "display_manager.h"
#include "entity_manager.h"

#include <math.h>
#include "misc.h"

static const float sphereRad = 0.5f;
static float maxspeed = 5.0f;

CActor::CActor()
{
	m_model = NULL;
	m_InContact = false;
	SetGhost(false);
	//SetHealth(100);
}

CActor::~CActor()
{
	if(m_model)
		delete m_model;
}

void CActor::Update(float diff)
{
	CEntity::Update(diff);

	if(m_model)
		m_model->Update(diff);

    SetPosition(
            GetPosition() + GetVelocity() * maxspeed * diff
            );
}

void CActor::SetGhost(bool ghost)
{
	// inform the other players of this change
	// this is not a request for other player data
	/*if(world.netDriver)
		world.netDriver->SendInfo(this, false);*/
	m_Ghost = ghost;
}

void CActor::Draw()
{
	world.display->DrawActor(this);
}

bool CActor::Collides(const Vector3f &futurePos, Vector3f &collisionPoint)
{
	return CheckTree(&world.tree->m_TopNode, world.tree->m_origin, 
				     world.tree->m_width, futurePos, collisionPoint);
}

void CActor::DoCollision(Vector3f &cr_position, Vector3f &new_velocity, const float diff, int depth/* = 0*/)
{
	float epsilon_cp = 0.0001f;
	//Have we collided more than three times recursivly?
	if(depth == 4) {
		// Lets just stop the player then (quite probably in a corner)
		new_velocity = Vector3f(0.0f, 0.0f, 0.0f);
		return;
	} else if(diff == 0 || new_velocity.Length() < epsilon_cp) {
		return;
	}
	


	Vector3f futurePos(new_velocity * diff);
	float vel_length = futurePos.Length();
	futurePos += cr_position;

	Vector3f collisionPoint;

	if(!CheckTree(&world.tree->m_TopNode, world.tree->m_origin, 
				  world.tree->m_width, futurePos, collisionPoint))
	{
		float len = new_velocity.Length() - epsilon_cp;
		new_velocity.Normalize();
		new_velocity *= len;
		if (depth == 0) 
			m_InContact = false;
		else 
			m_InContact = true;

		SetPosition(cr_position + new_velocity * diff);
		
		return; // No collision
	}

	m_InContact = true;

	Vector3f pretend_norm = futurePos - collisionPoint;
	pretend_norm.Normalize();
	pretend_norm *= (sphereRad + epsilon_cp);

	//Vector3f original_pos = cr_position;
	cr_position = collisionPoint + pretend_norm;
	Vector3f destPoint = cr_position + new_velocity;

	Vector3f slidePlaneOrigin = collisionPoint;
	Vector3f slidePlaneNormal = cr_position - collisionPoint;
	slidePlaneNormal.Normalize();

	// We now project the destination point onto the sliding plane
	double l = intersectRayPlane(destPoint, slidePlaneNormal, 
								slidePlaneOrigin, slidePlaneNormal); 

	Vector3f newDestinationPoint;
	newDestinationPoint = destPoint + (slidePlaneNormal * l);

	// Generate the slide vector, which will become our new velocity vector
	// for the next iteration

	Vector3f newVelocityVector = newDestinationPoint - collisionPoint;

	float ratio = (GetPosition() - cr_position).Length();

	if(vel_length >= 0.0f && ratio >= 0.0f) {
		ratio /= vel_length;
		ratio = 1 - ratio;
		
		cr_position += (newVelocityVector * diff) * ratio;

		DoCollision(cr_position, newVelocityVector, diff, depth + 1);
		if(depth == 0)
			SetPosition(cr_position);
	
	} else {
		ratio = 0.0f;
	}

	new_velocity = newVelocityVector;


}

// TODO: Check this function!
bool CActor::InNode(const Vector3f &center, const float &width, const Vector3f &futurePos) {
	Vector3f node_sphere_rad(width,width,width);
	float def_dist = 0.01f; // XXX: Check this?
	float dist_between = 0;
	//return true;

	// XXX: why do we check current position?
	//check current position
	dist_between = (center - GetPosition()).Length();
	dist_between -= node_sphere_rad.Length();
	dist_between -= def_dist;

	if(dist_between < 0.0f) 
		return true;

	// check future position
	dist_between = (center - futurePos).Length();
	dist_between -= node_sphere_rad.Length();
	dist_between -= def_dist;

	if(dist_between < 0.0f) 
		return true;

	return false;
}

bool CActor::CheckTree(const COctree::c_node *node, const Vector3f &offset, float width, 
					   const Vector3f &futurePos, Vector3f &collisionPoint)
{
	bool result = false;
	int x, y, z;

	// Do we intersect with this node at all?  If not, we can't collide with
	// it (return false)
	if(!InNode(offset, width, futurePos)) 
		return false;

	width /= 2.0f;
	
	// Check all children for intersections first
	for(x = 0; x < 2; x++) {
		for(y = 0; y < 2; y++) {
			for(z = 0; z < 2; z++) {
				if(node->m_children[x][y][z] != NULL) {
					result = CheckTree(node->m_children[x][y][z], 
							Vector3f((x==0) ? offset.x - width : offset.x + width, 
									 (y==0) ? offset.y - width : offset.y + width,
									 (z==0) ? offset.z - width : offset.z + width),
							width, futurePos, collisionPoint);

					if(result)
						return result;
				}
			}
		}
	}
	
	// Finally check for intersections with objects in this octree node.
	list<CMesh *>::const_iterator i = node->m_objs.begin();
	for(; i != node->m_objs.end(); ++i) {
		float depth;
		if( m_model->collider->Collides((*i)->collider, futurePos, collisionPoint, depth) )
		{
			return true;
		}

	}

	return false;
}
