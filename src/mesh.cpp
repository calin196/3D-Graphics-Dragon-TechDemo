#include "mesh.h"
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <fmt/format.h>
DISABLE_WARNINGS_POP()
#include <iostream>
#include <vector>

GPUMaterial::GPUMaterial(const Material& material)
    : kd(material.kd)
    , ks(material.ks)
    , shininess(material.shininess)
    , transparency(material.transparency)
{
}

GPUMesh::GPUMesh(const Mesh& cpuMesh, const std::vector<SkinInfluence>& skin,
    const std::vector<glm::vec3>& mouthOffsets)
{
    if (!skin.empty() && skin.size() != cpuMesh.vertices.size())
        throw std::invalid_argument("Skin influence count must match vertex count");
    if (!mouthOffsets.empty() && mouthOffsets.size() != cpuMesh.vertices.size()) {
        throw std::invalid_argument("Mouth offset count must match vertex count");
    }

    GPUMaterial gpuMaterial(cpuMesh.material);
    glGenBuffers(1, &m_uboMaterial);
    glBindBuffer(GL_UNIFORM_BUFFER, m_uboMaterial);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUMaterial), &gpuMaterial, GL_STATIC_READ);

    m_hasTextureCoords = static_cast<bool>(cpuMesh.material.kdTexture);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    const GLsizeiptr vertexBufferBytes =
        static_cast<GLsizeiptr>(cpuMesh.vertices.size() * sizeof(decltype(cpuMesh.vertices)::value_type));
    glBufferData(GL_ARRAY_BUFFER, vertexBufferBytes, cpuMesh.vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    const GLsizeiptr indexBufferBytes =
        static_cast<GLsizeiptr>(cpuMesh.triangles.size() * sizeof(decltype(cpuMesh.triangles)::value_type));
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferBytes, cpuMesh.triangles.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

    glVertexAttribDivisor(0, 0);
    glVertexAttribDivisor(1, 0);
    glVertexAttribDivisor(2, 0);

    if (!skin.empty()) {
        glGenBuffers(1, &m_skinVbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_skinVbo);
        const GLsizeiptr skinBufferBytes = static_cast<GLsizeiptr>(skin.size() * sizeof(SkinInfluence));
        glBufferData(GL_ARRAY_BUFFER, skinBufferBytes, skin.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(3, 2, GL_INT, sizeof(SkinInfluence), (void*)offsetof(SkinInfluence, bones));
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(SkinInfluence), (void*)offsetof(SkinInfluence, weights));
    }

    if (!mouthOffsets.empty()) {
        glGenBuffers(1, &m_mouthVbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_mouthVbo);
        const GLsizeiptr morphBufferBytes = static_cast<GLsizeiptr>(mouthOffsets.size() * sizeof(glm::vec3));
        glBufferData(GL_ARRAY_BUFFER, morphBufferBytes, mouthOffsets.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    }

    m_numIndices = static_cast<GLsizei>(3 * cpuMesh.triangles.size());
}

GPUMesh::GPUMesh(GPUMesh&& other)
{
    moveInto(std::move(other));
}

GPUMesh::~GPUMesh()
{
    freeGpuMemory();
}

GPUMesh& GPUMesh::operator=(GPUMesh&& other)
{
    moveInto(std::move(other));
    return *this;
}

std::vector<GPUMesh> GPUMesh::loadMeshGPU(std::filesystem::path filePath, bool normalize)
{
    if (!std::filesystem::exists(filePath))
        throw MeshLoadingException(fmt::format("File {} does not exist", filePath.string().c_str()));

    std::vector<Mesh> subMeshes = loadMesh(filePath, { .normalizeVertexPositions = normalize });
    std::vector<GPUMesh> gpuMeshes;
    for (const Mesh& mesh : subMeshes) {
        gpuMeshes.emplace_back(mesh);
    }

    return gpuMeshes;
}

bool GPUMesh::hasTextureCoords() const
{
    return m_hasTextureCoords;
}

void GPUMesh::draw(const Shader& drawingShader)
{

    drawingShader.bindUniformBlock("Material", 0, m_uboMaterial);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_numIndices, GL_UNSIGNED_INT, nullptr);
}

void GPUMesh::moveInto(GPUMesh&& other)
{
    freeGpuMemory();
    m_numIndices = other.m_numIndices;
    m_hasTextureCoords = other.m_hasTextureCoords;
    m_ibo = other.m_ibo;
    m_vbo = other.m_vbo;
    m_skinVbo = other.m_skinVbo;
    m_mouthVbo = other.m_mouthVbo;
    m_vao = other.m_vao;
    m_uboMaterial = other.m_uboMaterial;

    other.m_numIndices = 0;
    other.m_hasTextureCoords = other.m_hasTextureCoords;
    other.m_ibo = INVALID;
    other.m_vbo = INVALID;
    other.m_skinVbo = INVALID;
    other.m_mouthVbo = INVALID;
    other.m_vao = INVALID;
    other.m_uboMaterial = INVALID;
}

void GPUMesh::freeGpuMemory()
{
    if (m_vao != INVALID)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo != INVALID)
        glDeleteBuffers(1, &m_vbo);
    if (m_skinVbo != INVALID)
        glDeleteBuffers(1, &m_skinVbo);
    if (m_mouthVbo != INVALID)
        glDeleteBuffers(1, &m_mouthVbo);
    if (m_ibo != INVALID)
        glDeleteBuffers(1, &m_ibo);
    if (m_uboMaterial != INVALID)
        glDeleteBuffers(1, &m_uboMaterial);
}
