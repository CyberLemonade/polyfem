set(SOURCES
	autodiff.h
	AutodiffTypes.hpp
	base64Layer.cpp
	base64Layer.hpp
	Bessel.hpp
	BoundarySampler.cpp
	BoundarySampler.hpp
	Selection.cpp
	Selection.hpp
	DisableWarnings.hpp
	EdgeSampler.cpp
	EdgeSampler.hpp
	ElasticityUtils.cpp
	ElasticityUtils.hpp
	EnableWarnings.hpp
	getRSS.c
	HashUtils.hpp
	Logger.cpp
	Logger.hpp
	InterpolatedFunction.cpp
	InterpolatedFunction.hpp
	JSONUtils.cpp
	JSONUtils.hpp
	MatrixUtils.cpp
	MatrixUtils.hpp
	MaybeParallelFor.hpp
	MaybeParallelFor.tpp
	par_for.cpp
	par_for.hpp
	raster.cpp
	raster.hpp
	RBFInterpolation.cpp
	RBFInterpolation.hpp
	RefElementSampler.cpp
	RefElementSampler.hpp
	StringUtils.cpp
	StringUtils.hpp
	ExpressionValue.cpp
	ExpressionValue.hpp
	Timer.hpp
	Types.hpp
)

prepend_current_path(SOURCES)
polyfem_set_source_group(${SOURCES})
target_sources(polyfem PRIVATE ${SOURCES})
